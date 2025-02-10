# Redis Data Structure Technical Specification

Version: 1.0.0

## Overview

This document specifies the Redis data structure requirements for the RugPull Detector system. The system uses Redis Sorted Sets to store and retrieve time-series trading data for cryptocurrency tokens.

## Key Structure

### Format

```
recent_trades:<token_address>
```

### Example

```
recent_trades:BsfR9qUbypL6zb4geCJ6yqFvPJg4vF7DyXmY73nSpump
```

### Requirements

- Token address must be in base58 format
- Key TTL: 24 hours (86400 seconds)
- Maximum key length: 128 bytes

## Sorted Set Structure

### Score

- Type: Double-precision floating-point
- Format: Unix timestamp with microsecond precision
- Example: 1739184338.6333098
- Range: Current time ± 24 hours
- Precision requirements: Must maintain microsecond ordering

### Value

- Type: JSON string
- Encoding: UTF-8
- Maximum size: 2KB
- Compression: None

### Required JSON Fields

| Field        | Type   | Description           | Example            | Validation Rules    |
| ------------ | ------ | --------------------- | ------------------ | ------------------- |
| signature    | string | Transaction signature | "5PEnuUT..."       | Base58, 88 chars    |
| mint         | string | Token mint address    | "14k7Lp..."        | Base58, matches key |
| timestamp    | number | Unix timestamp        | 1739184338.6333098 | Must match score    |
| marketCapSol | number | Market cap in SOL     | 28.028628070587033 | > 0                 |
| solAmount    | number | Trade amount in SOL   | 0.001014365        | > 0                 |

### Optional JSON Fields

| Field                 | Type   | Description        | Example                            |
| --------------------- | ------ | ------------------ | ---------------------------------- |
| txType                | string | Transaction type   | "sell", "buy"                      |
| tokenAmount           | number | Token quantity     | 36189.104326                       |
| newTokenBalance       | number | Updated balance    | 0                                  |
| bondingCurveKey       | string | Pool address       | "2bQpbWV..."                       |
| vTokensInBondingCurve | number | Virtual tokens     | 1071666285.328535                  |
| vSolInBondingCurve    | number | Virtual SOL        | 30.03733572726111                  |
| pool                  | string | Pool type          | "pump"                             |
| human_readable_time   | string | ISO 8601 timestamp | "2025-02-10T10:45:38.633337+00:00" |
| traderPublicKey       | string | Trader's address   | "3c6KJR..."                        |

## Example Record

```json
{
  "signature": "5PEnuUTTiqyEoeXtSdFPC38pY1bFCXFAFXKq52WQBCUPzC8sJLhxNDxPDB9YBrQWZDxB6GTzJcPKH399ZpwUP1LY",
  "mint": "14k7LpDRKvyVuMWfGe1vB5ByoQh1yUQPetApMBLuzw1f",
  "traderPublicKey": "3c6KJRkWtYoGHokK1x1YrYUsNUT672J3RHRgsodhC6KJ",
  "txType": "sell",
  "tokenAmount": 36189.104326,
  "solAmount": 0.001014365,
  "newTokenBalance": 0,
  "bondingCurveKey": "2bQpbWV1FYiddhyEceQf3FuviVPd4JQKYnptHnCEGKR5",
  "vTokensInBondingCurve": 1071666285.328535,
  "vSolInBondingCurve": 30.03733572726111,
  "marketCapSol": 28.028628070587033,
  "pool": "pump",
  "timestamp": 1739184338.6333098,
  "human_readable_time": "2025-02-10T10:45:38.633337+00:00"
}
```

## Size Considerations

### Single Record

- Typical size: ~500 bytes
- Maximum size: 2KB
- Recommended size: <1KB

### Set Size

- Minimum records: 10
- Maximum records: 10,000
- Typical records: 100-1,000
- Retention period: 24 hours

## Performance Requirements

### Write Operations

- Maximum latency: 10ms
- Throughput: 1,000 writes/second per key
- Batch size: Up to 100 records

### Read Operations

- Maximum latency: 50ms
- Throughput: 100 reads/second per key
- Typical window size: 60 seconds of data

## Memory Usage

### Per Key

- Minimum: ~5KB (10 records)
- Typical: ~500KB (1,000 records)
- Maximum: ~20MB (10,000 records)

### Total Dataset

- Minimum memory: 1GB
- Recommended memory: 8GB
- Maximum memory: 32GB

## Data Consistency

### Ordering

- Records must be strictly ordered by timestamp
- No duplicate timestamps allowed
- Maximum allowed clock skew: 1 second

### Validation Rules

1. Timestamp in score must match timestamp in JSON
2. Token address in key must match mint in JSON
3. Market cap and trade amount must be positive
4. All numerical values must be finite and non-NaN

## Data Retention

### TTL Policy

- Individual records: No separate TTL
- Entire key: 24 hours
- Automatic cleanup: Yes, via Redis TTL

### Pruning Policy

1. Remove records older than 24 hours
2. Keep minimum 10 records per key
3. Hard limit of 10,000 records per key

## Error Handling

### Invalid Data

1. Skip malformed JSON records
2. Log validation failures
3. Continue processing valid records

### Missing Data

1. Minimum 10 records required for analysis
2. Maximum gap between records: 5 minutes
3. Log warnings for data gaps

## Monitoring

### Key Metrics

1. Records per key
2. Record insertion rate
3. Data age
4. Memory usage
5. Access patterns

### Alerts

1. High memory usage (>80%)
2. High insertion rate (>1000/sec)
3. Data gaps (>5 min)
4. TTL failures

## CLI Example

```bash
# Add trade record
redis-cli ZADD "recent_trades:TOKEN_ADDRESS" 1739184338.6333098 '{
    "signature": "5PEnuUT...",
    "mint": "TOKEN_ADDRESS",
    "marketCapSol": 28.028628070587033,
    "solAmount": 0.001014365,
    "timestamp": 1739184338.6333098
}'

# Get recent trades (last 60 seconds)
redis-cli ZRANGEBYSCORE "recent_trades:TOKEN_ADDRESS" \
    (NOW-60) NOW

# Get record count
redis-cli ZCARD "recent_trades:TOKEN_ADDRESS"

# Check TTL
redis-cli TTL "recent_trades:TOKEN_ADDRESS"
```
