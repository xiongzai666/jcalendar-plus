#pragma once

inline bool validPageId(int page) { return page >= 0 && page <= 3; }

inline int nextPageInOrder(int page, bool forward) {
    static const int order[] = {2, 1, 0, 3};
    for (int index = 0; index < 4; ++index)
        if (order[index] == page) return order[(index + (forward ? 1 : 3)) % 4];
    return 2;
}

inline int pageOnStartup(int retainedPage, int configuredPage, bool fromDeepSleep) {
    const int home = validPageId(configuredPage) ? configuredPage : 2;
    return fromDeepSleep && validPageId(retainedPage) ? retainedPage : home;
}
