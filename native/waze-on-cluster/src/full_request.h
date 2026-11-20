// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Robert Nisipeanu
/* Where else the cluster's map may go on the panel (README.md "Full cluster"):
 * an app's full-cluster ask (FullRequest) and a cluster layout's map window
 * (LayoutWindow). Both are files that the asking app keeps fresh while it
 * wants the map there; once the app stops, they go stale and the map is
 * back in its own place within seconds. Header only.
 *
 * FullRequest: the map over the whole cluster. The map covers the stock
 * gauges there, the speedometer among them, so an app that asks must draw the
 * speed itself, and the ask lasts only as long as that app is alive: it
 * rewrites full_request_file (by rename) at least every second while its own
 * readings are drawn on the cluster, with
 *
 *   {"v":1,"full":1,"app":"<its id>"}
 *
 * and the map is full only while the file's last change is at most
 * full_request_stale_ms (1500 by default) old. "full":0, a file that is not
 * a regular file (a symlink included), cannot be read or is over 1 KiB, or
 * none at all: not full.
 *
 * LayoutWindow: a cluster layout's map window (Cluster Layouts, ID7 Cluster
 * Themes, an app that only makes the map larger). While it shows, it keeps
 * layout_window_file (/dev/shm/cluster-info/map_window.txt) fresh, one line
 *
 *   panel X Y W H
 *
 * the rect on the 1920x720 panel it leaves for the map, which goes there,
 * over the layout's content and under its overlay class. Fresh: changed
 * within kFreshMs. Not a regular file, a symlink, another first word, more
 * than the rect, a rect outside the panel or under 64x32: no window. A window
 * comes before a full-cluster ask.
 *
 * Each file is looked at no more often than every kCheckMs; the answer in
 * between is the last one. */
#ifndef WOC_FULL_REQUEST_H
#define WOC_FULL_REQUEST_H

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "log.h"
#include "map_rect.h"

namespace woc {

namespace full_detail {

/* The file's text (at most max bytes) when it is a regular file, not a
 * symlink, changed within fresh_ms of now (either way); else false. */
inline bool read_fresh(const std::string &path, int64_t now_wall_ms, int64_t fresh_ms, size_t max, std::string *out)
{
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
        return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || (size_t)st.st_size > max) {
        close(fd);
        return false;
    }
    const int64_t age = now_wall_ms - ((int64_t)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000);
    if (age > fresh_ms || age < -fresh_ms) {
        close(fd);
        return false;
    }
    std::string text(max + 1, '\0');
    const ssize_t n = ::read(fd, &text[0], max + 1);
    close(fd);
    if (n <= 0 || (size_t)n > max)
        return false;
    text.resize((size_t)n);
    *out = text;
    return true;
}

/* The value after "key": in a flat JSON object: a string's text, or a
 * number's digits; empty when there is none. */
inline std::string json_value(const std::string &s, const char *key)
{
    const std::string k = std::string("\"") + key + "\"";
    size_t i = s.find(k);
    if (i == std::string::npos)
        return std::string();
    i += k.size();
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        i++;
    if (i >= s.size() || s[i++] != ':')
        return std::string();
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        i++;
    if (i < s.size() && s[i] == '"') {
        const size_t end = s.find('"', i + 1);
        return end == std::string::npos ? std::string() : s.substr(i + 1, end - i - 1);
    }
    size_t end = i;
    while (end < s.size() && s[end] >= '0' && s[end] <= '9')
        end++;
    return s.substr(i, end - i);
}

}  // namespace full_detail

class FullRequest {
public:
    static const int64_t kCheckMs = 250;

    /* path empty: never full. */
    FullRequest(const std::string &path, int64_t stale_ms) : path_(path), stale_ms_(stale_ms) {}

    /* now_wall_ms: the wall clock (the file's change time is one), in ms. */
    bool wanted(int64_t now_wall_ms)
    {
        if (path_.empty())
            return false;
        if (checked_ && now_wall_ms >= checked_ms_ && now_wall_ms - checked_ms_ < kCheckMs)
            return wanted_;
        checked_ = true;
        checked_ms_ = now_wall_ms;
        const bool was = wanted_;
        const std::string was_app = app_;
        std::string text;
        wanted_ = full_detail::read_fresh(path_, now_wall_ms, stale_ms_, 1024, &text) &&
                  text.find('{') != std::string::npos && full_detail::json_value(text, "full") == "1";
        const std::string app = wanted_ ? full_detail::json_value(text, "app") : std::string();
        app_ = !wanted_ ? std::string() : !app.empty() && app.size() <= 64 ? app : "an app";
        if (wanted_ != was)
            log_info("full cluster: %s%s%s", wanted_ ? "asked by " : "not asked any more", wanted_ ? app_.c_str() : "",
                     wanted_ ? ": the map over the whole cluster" : ": the map in the map area");
        else if (wanted_ && app_ != was_app)
            log_info("full cluster: asked by %s now", app_.c_str());
        return wanted_;
    }
    /* The app that asked, while it is full; empty otherwise. */
    const std::string &app() const { return app_; }

private:
    std::string path_;
    int64_t stale_ms_;
    bool checked_ = false;   /* looked at once at least */
    int64_t checked_ms_ = 0;
    bool wanted_ = false;
    std::string app_;
};

class LayoutWindow {
public:
    static const int64_t kFreshMs = 3500;

    explicit LayoutWindow(const std::string &path) : path_(path) {}

    /* The window while there is one, into *out. */
    bool read(int64_t now_wall_ms, MapRect *out)
    {
        if (path_.empty())
            return false;
        if (!(checked_ && now_wall_ms >= checked_ms_ && now_wall_ms - checked_ms_ < FullRequest::kCheckMs)) {
            checked_ = true;
            checked_ms_ = now_wall_ms;
            MapRect r;
            const bool held = look(now_wall_ms, &r);
            const bool moved = held && (r.x != rect_.x || r.y != rect_.y || r.w != rect_.w || r.h != rect_.h);
            if (held && (moved || !held_))
                log_info("layout window: the map at %u,%u %ux%u on the panel", r.x, r.y, r.w, r.h);
            else if (!held && held_)
                log_info("layout window: gone, the map back to its own place");
            held_ = held;
            rect_ = held ? r : MapRect();
        }
        if (held_)
            *out = rect_;
        return held_;
    }

private:
    bool look(int64_t now_wall_ms, MapRect *out) const
    {
        std::string text;
        if (!full_detail::read_fresh(path_, now_wall_ms, kFreshMs, 80, &text))
            return false;
        char word[16] = {0}, tail[2] = {0};
        unsigned x, y, w, h;
        if (sscanf(text.c_str(), "%15s %u %u %u %u %1s", word, &x, &y, &w, &h, tail) != 5 ||
            std::string(word) != "panel")
            return false;
        MapRect r;
        r.x = x;
        r.y = y;
        r.w = w;
        r.h = h;
        if (w < 64 || h < 32 || !r.fits(1920, 720))
            return false;
        *out = r;
        return true;
    }

    std::string path_;
    bool checked_ = false;
    int64_t checked_ms_ = 0;
    bool held_ = false;
    MapRect rect_;
};

}  // namespace woc

#endif
