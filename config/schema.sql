-- HTKIS HyperDisk X — PostgreSQL Schema (Phase 1)
-- Version: 0.1.0

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- 镜像表
CREATE TABLE images (
    image_id    BIGSERIAL PRIMARY KEY,
    name        VARCHAR(256) NOT NULL,
    total_size  BIGINT NOT NULL,
    block_count INTEGER NOT NULL,
    os_layer_id      INTEGER,
    driver_layer_id  INTEGER,
    app_layer_id     INTEGER,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(name)
);

-- 镜像层表
CREATE TABLE layers (
    layer_id    SERIAL PRIMARY KEY,
    image_id    BIGINT NOT NULL REFERENCES images(image_id) ON DELETE CASCADE,
    layer_type  SMALLINT NOT NULL,
    total_size  BIGINT NOT NULL,
    block_count INTEGER NOT NULL,
    ref_count   INTEGER NOT NULL DEFAULT 0,
    block_map_hash BYTEA,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 终端表
CREATE TABLE terminals (
    terminal_id     BIGSERIAL PRIMARY KEY,
    hostname        VARCHAR(256),
    dna_group_id    INTEGER,
    status          VARCHAR(32) NOT NULL DEFAULT 'offline',
    last_heartbeat  TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- DNA 分组表
CREATE TABLE dna_groups (
    dna_group_id   SERIAL PRIMARY KEY,
    dna_digest     BYTEA NOT NULL,
    driver_layer_id INTEGER,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(dna_digest)
);

-- 快照表
CREATE TABLE snapshots (
    snapshot_id     BIGSERIAL PRIMARY KEY,
    image_id        BIGINT NOT NULL REFERENCES images(image_id),
    name            VARCHAR(256) NOT NULL,
    os_layer_ver    INTEGER NOT NULL,
    driver_layer_ver INTEGER,
    app_layer_ver   INTEGER,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 灰度策略表
CREATE TABLE canary_strategies (
    strategy_id     BIGSERIAL PRIMARY KEY,
    target_image_id BIGINT NOT NULL REFERENCES images(image_id),
    state           VARCHAR(32) NOT NULL DEFAULT 'draft',
    current_batch   INTEGER NOT NULL DEFAULT 0,
    total_batches   INTEGER NOT NULL,
    fault_threshold DOUBLE PRECISION NOT NULL DEFAULT 0.05,
    window_start    TIMESTAMPTZ,
    window_end      TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 服务器节点表
CREATE TABLE server_nodes (
    node_id         SERIAL PRIMARY KEY,
    hostname        VARCHAR(256) NOT NULL,
    listen_addr     VARCHAR(64) NOT NULL,
    role            VARCHAR(32) NOT NULL,
    status          VARCHAR(32) NOT NULL DEFAULT 'active',
    capacity_bytes  BIGINT,
    used_bytes      BIGINT DEFAULT 0,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- 租户表
CREATE TABLE tenants (
    tenant_id   SERIAL PRIMARY KEY,
    name        VARCHAR(256) NOT NULL,
    config      JSONB NOT NULL DEFAULT '{}',
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(name)
);

CREATE INDEX idx_images_name ON images(name);
CREATE INDEX idx_layers_image ON layers(image_id);
CREATE INDEX idx_terminals_status ON terminals(status);
CREATE INDEX idx_terminals_dna ON terminals(dna_group_id);
CREATE INDEX idx_snapshots_image ON snapshots(image_id);
CREATE INDEX idx_canary_state ON canary_strategies(state);
CREATE INDEX idx_server_nodes_role ON server_nodes(role);
