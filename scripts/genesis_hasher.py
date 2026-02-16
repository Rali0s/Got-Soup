#!/usr/bin/env python3
"""
External genesis hasher for got-soup.

Usage:
  python3 scripts/genesis_hasher.py
"""

from __future__ import annotations

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


def main() -> None:
    mainnet = GenesisSpec(
        chain_id="got-soup-mainnet-v1",
        network_id="mainnet",
        psz_timestamp="Feb. 16 2026 - 07:18 - 1771244337 - 'Europe's earnings gain pace while lofty valuations cap rewards' - https://www.reuters.com/business/finance/europes-earnings-gain-pace-while-lofty-valuations-cap-rewards-2026-02-16/",
        seed_peers=["seed.got-soup.local:4001","24.188.147.247:4001"],
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
    print(json.dumps(out, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
