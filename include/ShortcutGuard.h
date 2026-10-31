#pragma once

// Remember the whole key gesture: releasing modifiers before S must not turn
// a system shortcut into an ordinary save. No dependency on a window backend.
class ShortcutGuard {
public:
    static bool plain(bool focused, bool modified) { return focused && !modified; }
    void cancel() { saveArmed_ = false; }
    void pressSave(bool focused, bool modified) { saveArmed_ = plain(focused, modified); }
    bool releaseSave(bool focused, bool modified) {
        const bool save = saveArmed_ && plain(focused, modified);
        cancel();
        return save;
    }
private:
    bool saveArmed_ = false;
};
