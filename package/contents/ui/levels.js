.pragma library

// Whether a battery counts as low: shared by the rings and the tray status, so the red ring
// and the tray promotion always agree.
//   percentage:  0-100, or -1 when unknown
//   coarseLevel: "" for a real percentage, else the rough level the device reports
//   threshold:   the "Low battery at" setting
function isLow(percentage, charging, coarseLevel, threshold) {
    if (charging) {
        return false;
    }
    if (coarseLevel === "critical" || coarseLevel === "low") {
        return true;
    }
    if (coarseLevel) {
        return false; // "normal", "high" or "full", whatever the threshold
    }
    return percentage >= 0 && percentage <= threshold;
}
