#include "dockprogress.h"

// No-op implementations for non-macOS platforms
void setDockBadge(const QString &) {}
void clearDockBadge() {}
void claimFileType() {}
