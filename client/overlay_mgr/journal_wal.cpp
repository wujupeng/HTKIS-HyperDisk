namespace hd::overlay {
class JournalWal { public: bool Append(const void*, size_t) { return true; } bool Recover() { return true; } };
} // namespace hd::overlay
