#ifndef DOCKPROGRESS_H
#define DOCKPROGRESS_H

#include <QString>

// macOS Dock tile badge for progress indication.
// On non-macOS platforms, these are no-ops.

void setDockBadge(const QString &text);
void clearDockBadge();

#endif // DOCKPROGRESS_H
