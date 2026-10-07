#pragma once
inline bool weatherStartAllowed(int weatherState,int timeState,bool portalActive,bool portalVisible) {
    return weatherState == -1 && timeState > 0 && !portalActive && !portalVisible;
}
// Publish a terminal failure; never replace a task that still owns resources.
template<class Factory,class Handle,class Status>
bool startTaskIfIdle(const Factory& create,Handle& handle,Status& status) {
    if (handle) return false;
    status = 0;
    if (create(&handle)) return true;
    handle = nullptr;
    status = 2;
    return false;
}
