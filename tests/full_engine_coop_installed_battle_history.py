"""Read-only native battle-outcome evidence for installed campaign qualifiers.

The caller captures History.dat after Resume and before battle, then reads the
same private VFS file after native combat. This witness does not itself prove a
checkpoint, reconnect, tactical contribution, or successful mercenary arrival.
"""

from __future__ import annotations

import hashlib
import struct

from full_engine_coop_installed_movement import MovementFailure


# Laptop/history.h; CampaignLedgerRecord::HistoryFields writes these scalar
# fields in host byte order, separately, without structure padding.
HISTORY_WONBATTLE = 25
HISTORY_LOSTTOWNSECTOR = 64
HISTORY_DEFENDEDTOWNSECTOR = 65
HISTORY_LOSTBATTLE = 66
HISTORY_FATALAMBUSH = 67
HISTORY_WIPEDOUTENEMYAMBUSH = 68
HISTORY_UNSUCCESSFULATTACK = 69
HISTORY_SUCCESSFULATTACK = 70
HISTORY_KILLEDBYBLOODCATS = 72
HISTORY_SLAUGHTEREDBLOODCATS = 73
HISTORY_RECORD = struct.Struct("=BBIhhbB")
MAX_HISTORY_RECORDS = 100_000
BATTLE_RESULTS = frozenset((HISTORY_WONBATTLE, HISTORY_LOSTTOWNSECTOR,
                           HISTORY_DEFENDEDTOWNSECTOR, HISTORY_LOSTBATTLE,
                           HISTORY_FATALAMBUSH, HISTORY_WIPEDOUTENEMYAMBUSH,
                           HISTORY_UNSUCCESSFULATTACK, HISTORY_SUCCESSFULATTACK,
                           HISTORY_KILLEDBYBLOODCATS, HISTORY_SLAUGHTEREDBLOODCATS))


class BattleHistoryFailure(MovementFailure):
    pass


def decode_history(data):
    if not isinstance(data, bytes) or len(data) % HISTORY_RECORD.size:
        raise BattleHistoryFailure("native history is not a complete scalar record stream")
    if len(data) > MAX_HISTORY_RECORDS * HISTORY_RECORD.size:
        raise BattleHistoryFailure("native history exceeds the qualification record bound")
    fields = ("code", "second_code", "minute", "x", "y", "z", "color")
    return [dict(zip(fields, values)) for values in HISTORY_RECORD.iter_unpack(data)]


def _observe_defensive_result(before, after, *, sector, earliest_minute, latest_minute, expected_code):
    """Require one new native defense win at this battle's sector/time.

    Return None while no relevant result exists. Earlier wins, zero visible
    enemies, combat ending, and a generic post-combat checkpoint are insufficient.
    Any contradictory outcome or rewritten/truncated history fails explicitly.
    """
    if (not isinstance(sector, tuple) or len(sector) != 3 or
            any(type(value) is not int for value in sector) or
            not 1 <= sector[0] <= 16 or not 1 <= sector[1] <= 16 or sector[2] != 0 or
            type(earliest_minute) is not int or type(latest_minute) is not int or
            not 0 <= earliest_minute <= latest_minute <= 0xFFFFFFFF):
        raise BattleHistoryFailure("defensive history requires a bounded surface battle context")
    original, current = decode_history(before), decode_history(after)
    if not after.startswith(before):
        raise BattleHistoryFailure("native history before this battle was rewritten or truncated")
    appended = current[len(original):]
    results = [(len(original) + index, row) for index, row in enumerate(appended)
               if row["code"] in BATTLE_RESULTS and (row["x"], row["y"], row["z"]) == sector]
    if not results:
        return None
    if len(results) != 1:
        raise BattleHistoryFailure("native history has multiple battle outcomes for this sector")
    index, result = results[0]
    if not earliest_minute <= result["minute"] <= latest_minute:
        raise BattleHistoryFailure("native battle outcome lies outside the observed battle time")
    if result["code"] != expected_code:
        raise BattleHistoryFailure("native history did not record a successful town defense" if expected_code == HISTORY_DEFENDEDTOWNSECTOR
                                   else "native history did not record this town sector loss")
    if result["second_code"] != 0 or result["color"] != 0:
        raise BattleHistoryFailure("native defense result has unexpected auxiliary fields")
    return {"record_index": index, "record": result,
            "records_before": len(original), "records_after": len(current),
            "history_before_sha256": hashlib.sha256(before).hexdigest(),
            "history_after_sha256": hashlib.sha256(after).hexdigest(),
            "appended_records": appended}


def observe_defensive_victory(before, after, *, sector, earliest_minute, latest_minute):
    return _observe_defensive_result(before, after, sector=sector, earliest_minute=earliest_minute,
                                     latest_minute=latest_minute, expected_code=HISTORY_DEFENDEDTOWNSECTOR)


def observe_defensive_defeat(before, after, *, sector, earliest_minute, latest_minute):
    """A new town-sector loss is evidence for defeat return only, never victory."""
    return _observe_defensive_result(before, after, sector=sector, earliest_minute=earliest_minute,
                                     latest_minute=latest_minute, expected_code=HISTORY_LOSTTOWNSECTOR)
