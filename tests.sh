#!/bin/bash

OUT="results/pivot_scale.csv"
echo "n,p,h,time" > "$OUT"

P=(1 2 4 8 16 32)
H=(1 2 3)

for h in "${H[@]}"; do
        for p in "${P[@]}"; do
                result=$(mpirun --bind-to none -np "$p" ./quicksort "/proj/uppmax2026-1-92/A3/inputs/input125000000.txt" "output.txt" "$h")
                echo "$result" >> "$OUT"
        done
done

OUT="results/pivot_scale_backwards.csv"
echo "n,p,h,time" > "$OUT"

P=(1 2 4 8 16 32)
H=(1 2 3)

for h in "${H[@]}"; do
        for p in "${P[@]}"; do
                result=$(mpirun --bind-to none -np "$p" ./quicksort "/proj/uppmax2026-1-92/A3/inputs/backwards/input_backwards125000000.txt" "output.txt" "$h")
                echo "$result" >> "$OUT"
        done
done
