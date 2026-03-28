#ifndef DOCKPROGRESS_H
#define DOCKPROGRESS_H

#include <QString>

// macOS platform integration.
// On non-macOS platforms, these are no-ops.

void setDockBadge(const QString &text);
void clearDockBadge();
void claimFileType();

#endif // DOCKPROGRESS_H
