# Performance Figures (Current Code)

## Single Runs (Project Cases)

| Algo | Case | Exit | SLA0 | SLA1 | Energy (kWh) | Sim Runtime (s) | Pass |
|---|---:|---:|---:|---:|---:|---:|---:|
| ConservativeSpread | match.md | 0 | 0 | 0 | 0.0506302 | 21.24 | YES |
| ConservativeSpread | day.md | 0 | 0 | 0 | 64.1455 | 86400.7 | YES |
| ConservativeSpread | gentler_hour.md | 0 | 0 | 0 | 7.25969 | 3603.48 | YES |
| ConservativeSpread | smooth.md | 0 | 0 | 0 | 0.0121098 | 16.32 | YES |
| ConsolidationSleep | match.md | 0 | 0 | 0 | NA | NA | YES |
| ConsolidationSleep | day.md | 0 | 0 | 0 | NA | NA | YES |
| ConsolidationSleep | gentler_hour.md | 0 | 0 | 0 | NA | NA | YES |
| ConsolidationSleep | smooth.md | 0 | 0 | 0 | NA | NA | YES |
| ThresholdSLA | match.md | 0 | 0 | 0 | NA | NA | YES |
| ThresholdSLA | day.md | 0 | 0 | 0 | NA | NA | YES |
| ThresholdSLA | gentler_hour.md | 0 | 0 | 0 | NA | NA | YES |
| ThresholdSLA | smooth.md | 0 | 0 | 0 | NA | NA | YES |
| Greedy | match.md | 0 | 0 | 0 | 0.0506302 | 21.24 | YES |
| Greedy | day.md | 0 | 0 | 0 | 64.1455 | 86400.7 | YES |
| Greedy | gentler_hour.md | 0 | 0 | 0 | 7.25969 | 3603.48 | YES |
| Greedy | smooth.md | 0 | 0 | 0 | 0.0121098 | 16.32 | YES |
| AdaptiveHybrid | match.md | 0 | 0 | 0 | NA | NA | YES |
| AdaptiveHybrid | day.md | 0 | 0 | 0 | NA | NA | YES |
| AdaptiveHybrid | gentler_hour.md | 0 | 0 | 0 | NA | NA | YES |
| AdaptiveHybrid | smooth.md | 0 | 0 | 0 | NA | NA | YES |

## Seeded Averages (Algos 2/3/5)

| Algo | Case | Runs | Pass Runs | Pass Rate | Avg SLA0 | Avg SLA1 | Avg Energy (kWh) | Avg Sim Runtime (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ConsolidationSleep | match.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 0.048921 | 20.550000 |
| ConsolidationSleep | day.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 64.011750 | 86400.750000 |
| ConsolidationSleep | gentler_hour.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 7.255525 | 3604.230000 |
| ConsolidationSleep | smooth.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 0.011552 | 15.570000 |
| ThresholdSLA | match.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 0.048749 | 20.550000 |
| ThresholdSLA | day.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 64.011750 | 86400.750000 |
| ThresholdSLA | gentler_hour.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 7.255150 | 3604.230000 |
| ThresholdSLA | smooth.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 0.011552 | 15.570000 |
| AdaptiveHybrid | match.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 0.049129 | 20.580000 |
| AdaptiveHybrid | day.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 64.011750 | 86400.750000 |
| AdaptiveHybrid | gentler_hour.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 7.255650 | 3604.230000 |
| AdaptiveHybrid | smooth.md | 2 | 2 | 1.00 | 0.000000 | 0.000000 | 0.011552 | 15.570000 |
