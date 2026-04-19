#!/usr/bin/env python3
"""
External genesis hasher for got-soup.

Usage:
  python3 scripts/genesis_hasher.py
"""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass, asdict


@dataclass
class GenesisSpec:
    chain_id: str
    network_id: str
    psz_timestamp: str
    seed_peers: list[str]
    initial_allocations: list[tuple[str, int]]


def sha256_hex(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def compute(spec: GenesisSpec) -> dict[str, object]:
    alloc_repr = ",".join(f"{who}:{amount}" for who, amount in spec.initial_allocations) or "none"
    payload = "\n".join(
        [
            f"chain_id={spec.chain_id}",
            f"network={spec.network_id}",
            f"pszTimestamp={spec.psz_timestamp}",
            "seed_peers=" + ",".join(spec.seed_peers),
            f"initial_allocations={alloc_repr}",
        ]
    )
    merkle_root = sha256_hex(payload)
    block_hash = sha256_hex(
        "index=0|prev=genesis|"
        f"merkle={merkle_root}|"
        f"psz={spec.psz_timestamp}|"
        f"chain={spec.chain_id}|"
        f"network={spec.network_id}"
    )
    return {
        **asdict(spec),
        "payload": payload,
        "merkle_root": merkle_root,
        "block_hash": block_hash,
    }


def allocation_arg(value: str) -> tuple[str, int]:
    if ":" not in value:
        raise argparse.ArgumentTypeError("allocations must use identity:amount")
    identity, amount = value.split(":", 1)
    try:
        parsed = int(amount)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("allocation amount must be an integer") from exc
    return identity, parsed


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compute got-soup genesis hashes.")
    parser.add_argument("--chain-id")
    parser.add_argument("--network-id")
    parser.add_argument("--psz-timestamp")
    parser.add_argument("--seed-peer", action="append", default=[])
    parser.add_argument("--allocation", action="append", default=[], type=allocation_arg)
    parser.add_argument("--pretty", action="store_true", help="pretty-print JSON output")
    return parser.parse_args()


def default_specs() -> dict[str, dict[str, object]]:
    mainnet = GenesisSpec(
        chain_id="got-soup-mainnet-v3",
        network_id="mainnet",
        psz_timestamp="Apr 14, 2026, 5:08 AM ET//Anthropic discussing its powerful AI model Mythos with US: report//https://seekingalpha.com/news/4574628-anthropic-discussing-its-powerful-ai-model-mythos-with-us",
        seed_peers=["24.188.147.247:4001"],
        initial_allocations=[],
    )
    testnet = GenesisSpec(
        chain_id="got-soup-testnet-v1",
        network_id="testnet",
        psz_timestamp="Got Soup::P2P Tomato Soup testnet genesis | 2026-02-14",
        seed_peers=["seed.got-soup.local:14001"],
        initial_allocations=[],
    )
    out = {
        "mainnet": compute(mainnet),
        "testnet": compute(testnet),
    }
    return out


def main() -> None:
    args = parse_args()
    if args.chain_id and args.network_id and args.psz_timestamp:
        spec = GenesisSpec(
            chain_id=args.chain_id,
            network_id=args.network_id,
            psz_timestamp=args.psz_timestamp,
            seed_peers=args.seed_peer,
            initial_allocations=args.allocation,
        )
        payload = compute(spec)
    else:
        payload = default_specs()
    print(json.dumps(payload, indent=2 if args.pretty or not args.chain_id else None, sort_keys=True))


if __name__ == "__main__":
    main()
