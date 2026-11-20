# Changelog

## Unreleased

## 0.11.0

CarPlay's cluster map patch is applied in CarPlay's memory each time it
starts, and `/usr/bin/carplay` stays as it ships: no write of the system
partition to patch, remove or recover it. Updating from an earlier version
puts the original file back and keeps the map, with at most one head-unit
restart; where CarPlay already runs the earlier patch, none.
Full cluster: while an app asks (README.md "Full cluster"), the cluster map
covers the whole cluster, with the app drawing the speed and engine speed
over it. The map is back between the stock gauges within 1.5 s of the app
stopping.

A cluster layout's map window (`/dev/shm/cluster-info/map_window.txt`, as
Cluster Layouts and ID7 Cluster Themes write it): the map fills the window the
layout leaves for it, on the whole cluster.

## 0.10.12

The diagnostic journal is kept in memory and written to storage only around
failures and at shutdown, which greatly reduces writes to the head unit's
storage.

