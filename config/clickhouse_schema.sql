-- HTKIS HyperDisk X — ClickHouse Schema (Phase 1)

CREATE TABLE IF NOT EXISTS block_access_stats (
    terminal_id   UInt64,
    image_id      UInt64,
    block_offset  UInt64,
    layer_id      UInt8,
    access_time   DateTime64(3),
    is_cache_hit  UInt8,
    latency_us    UInt32,
    node_id       UInt32
)
ENGINE = MergeTree()
PARTITION BY toYYYYMMDD(access_time)
ORDER BY (image_id, block_offset, access_time)
TTL access_time + INTERVAL 30 DAY;

CREATE MATERIALIZED VIEW IF NOT EXISTS cache_stats_mv
ENGINE = SummingMergeTree()
PARTITION BY toYYYYMMDD(access_time)
ORDER BY (image_id, access_time)
TTL access_time + INTERVAL 30 DAY
AS SELECT
    image_id,
    toStartOfMinute(access_time) AS access_time,
    count() AS total_access,
    sum(is_cache_hit) AS cache_hits,
    avg(latency_us) AS avg_latency_us
FROM block_access_stats
GROUP BY image_id, access_time;
