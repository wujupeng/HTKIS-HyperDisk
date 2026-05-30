namespace hd::cache {
struct CachePolicy { virtual bool ShouldEvict() = 0; };
struct LruPolicy : CachePolicy { bool ShouldEvict() override { return true; } };
} // namespace hd::cache
