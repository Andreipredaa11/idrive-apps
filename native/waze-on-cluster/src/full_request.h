// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Robert Nisipeanu
/* FullRequest: whether an app asks for the cluster map over the whole cluster
 * (README.md "Full cluster").
 *
 * The cluster map covers the stock gauges there, the speedometer among them,
 * so an app that asks must draw the speed itself, and the ask lasts only as
 * long as that app is alive: it rewrites full_request_file (by rename) at
 * least every second while its own speed and engine speed are drawn on the
 * cluster, with a flat JSON object
 *
 *   {"v":1,"full":1,"app":"<its id>"}
 *
 * and the map is full only while the file's last change is at most
 * full_request_stale_ms (1500 by default) old. An app that stops, hangs or
 * is removed stops writing, and the map is back in the map area, between the
 * stock gauges, within that time. "full":0, a file that is not a regular
 * file, cannot be read or does not parse, or none at all: not full.
 *
 * The file is looked at no more often than every kCheckMs; the answer in
 * between is the last one, so a stale file is seen at most kCheckMs late. */
#ifndef WOC_FULL_REQUEST_H
#define WOC_FULL_REQUEST_H

#include <stdint.h>

#include <string>

#include "map_rect.h"

namespace woc {

class FullRequest {
public:
    static const int64_t kCheckMs = 250;

    /* path empty: never full. */
    FullRequest(const std::string &path, int64_t stale_ms) : path_(path), stale_ms_(stale_ms) {}

    /* now_wall_ms: the wall clock (the file's change time is one), in ms. */
    bool wanted(int64_t now_wall_ms);
    /* The app that asked, while it is full; empty otherwise. */
    const std::string &app() const { return app_; }

private:
    bool read(int64_t now_wall_ms);

    std::string path_;
    int64_t stale_ms_;
    bool checked_ = false;   /* looked at once at least */
    int64_t checked_ms_ = 0;
    bool wanted_ = false;
    std::string app_;
};

/* LayoutWindow: a cluster layout's map window (Cluster Layouts, ID7 Cluster
 * Themes). While the layout shows, it keeps layout_window_file
 * (/dev/shm/cluster-info/map_window.txt) fresh, one line
 *
 *   panel X Y W H
 *
 * the rect on the 1920x720 panel it leaves for the map, which goes there,
 * over the layout's content and under its overlay class (the layout draws the
 * speed itself). Fresh: changed within kFreshMs (the layouts touch it every
 * second or faster). Not a regular file, a symlink, another first word, a
 * rect outside the panel or under 64x32: no window. Looked at no more often
 * than FullRequest::kCheckMs. A window comes before a full-cluster ask. */
class LayoutWindow {
public:
    static const int64_t kFreshMs = 3500;

    explicit LayoutWindow(const std::string &path) : path_(path) {}
    /* The window while there is one, into *out. */
    bool read(int64_t now_wall_ms, MapRect *out);

private:
    bool look(int64_t now_wall_ms, MapRect *out);

    std::string path_;
    bool checked_ = false;
    int64_t checked_ms_ = 0;
    bool held_ = false;
    MapRect rect_;
};

}  // namespace woc

#endif
