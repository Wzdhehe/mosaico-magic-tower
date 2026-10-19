#!/usr/bin/env python3
"""Negative save/model contracts against an existing authored campaign trace.

Run verify_campaign_design.py first. This script adds no maps or route choices;
it replays its secrets_and_errands.json action log through the actual patched C.
No headless model assertion is a claim about production UI integration.
"""
import ctypes as C
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'assets_src/level_design'))
from campaign_rules import transform, HIDDEN, QUESTS
from verify_campaign_design import FLOORS
from tower_design_lab import build, Replay

OUT = ROOT/'artifacts/campaign-contract'
TRACE = ROOT/'artifacts/campaign-production/secrets_and_errands.json'


def instrument(source):
    return transform(source)+'''
unsigned contract_save_bytes(void) {return TOWER_SAVE_BYTES;}
unsigned contract_flag_bytes(void) {return TOWER_FLAG_BYTES;}
unsigned contract_bitmap_width(void) {return TOWER_MAX_W;}
unsigned contract_bitmap_offset(void) {return SAVE_BITMAP_OFFSET;}
'''


def checksum(blob):
    value = 2166136261
    for index, byte in enumerate(blob):
        if not 8 <= index <= 11:
            value = ((value ^ byte)*16777619) & 0xffffffff
    struct.pack_into('<I',blob,8,value)
    return blob


def main():
    assert TRACE.is_file(), 'Run tools/verify_campaign_design.py first'
    trace = json.loads(TRACE.read_text(encoding='utf-8'))
    lib = build(FLOORS,OUT,instrument)
    for name in ('contract_save_bytes','contract_flag_bytes','contract_bitmap_width','contract_bitmap_offset'):
        getattr(lib,name).argtypes = []
        getattr(lib,name).restype = C.c_uint
    save_size = lib.contract_save_bytes()
    flag_stride = lib.contract_flag_bytes()
    bitmap_width = lib.contract_bitmap_width()
    bitmap_offset = lib.contract_bitmap_offset()
    lib.tower_export.argtypes = [C.c_void_p,C.POINTER(C.c_uint8),C.c_size_t]
    lib.tower_import.argtypes = [C.c_void_p,C.POINTER(C.c_uint8),C.c_size_t]
    lib.tower_tile.argtypes = [C.c_void_p,C.c_uint,C.c_int,C.c_int]
    lib.tower_quest_state.argtypes = [C.c_void_p,C.c_uint]
    lib.tower_sigils.argtypes = [C.c_void_p]
    lib.tower_sluice_south.argtypes = [C.c_void_p]
    count = 0

    def export(handle):
        values = (C.c_uint8*save_size)()
        assert lib.tower_export(handle,values,save_size) == 0
        return bytes(values)

    def restore(handle,blob):
        values = (C.c_uint8*save_size).from_buffer_copy(blob)
        return lib.tower_import(handle,values,save_size)

    def flag(blob,position,value):
        floor,x,y = position
        bit = y*bitmap_width+x
        offset = bitmap_offset+floor*flag_stride+bit//8
        if value:
            blob[offset] |= 1 << (bit%8)
        else:
            blob[offset] &= ~(1 << (bit%8))

    with Replay(lib,FLOORS,OUT,'contract_trace') as replay:
        scratch = C.c_void_p()
        assert lib.tower_create(None,C.byref(scratch)) == 0
        seen_hidden, delivered, seen_bosses = set(), set(), set()
        valve_snapshot = None
        preview_checked = False
        hidden_lookup = {(f,*mouth): rects for f,mouth,rects in HIDDEN}
        quest_lookup = {npc: q for q,(npc,_,_) in enumerate(QUESTS)}
        boss_locations = {(9,5,3),(19,11,3),(29,13,5),(39,1,3),(49,5,3)}
        initial = export(replay.handle)

        def rejected_atomically(blob):
            nonlocal count
            before = export(scratch)
            assert restore(scratch,checksum(bytearray(blob))) == -3, 'forged save accepted'
            assert export(scratch) == before, 'failed import changed the live handle'
            count += 1

        try:
            assert lib.tower_tile(replay.handle,46,9,11) == 1, 'three-mark gate visible without marks'
            forged = bytearray(initial)
            flag(forged,(46,9,11),True)
            rejected_atomically(forged)
            for q,(npc,item,_) in enumerate(QUESTS):
                forged = bytearray(initial)
                flag(forged,npc,True)
                rejected_atomically(forged)

            for event in trace['actions']:
                action = event['action']
                before = replay.state()
                assert before == event['before'], ('trace drift before action',action)
                position = None
                before_blob = None
                if action[0] == 'move':
                    position = (before['floor'],before['x']+action[1],before['y']+action[2])
                    if position in hidden_lookup and position not in seen_hidden:
                        before_blob = export(replay.handle)
                        for x1,y1,x2,y2 in hidden_lookup[position]:
                            for y in range(y1,y2+1):
                                for x in range(x1,x2+1):
                                    assert lib.tower_tile(replay.handle,position[0],x,y) == 1, ('hidden room leaked before entry',position,x,y)
                    result = lib.tower_move(replay.handle,*action[1:])
                elif action[0] == 'confirm':
                    result = lib.tower_confirm(replay.handle)
                elif action[0] == 'buy':
                    result = lib.tower_buy(replay.handle,action[1])
                elif action[0] == 'warp':
                    result = lib.tower_teleport(replay.handle,action[1])
                elif action[0] == 'preview':
                    replay.preview(action[1])
                    result = 0
                else:
                    raise AssertionError(('unsupported authored trace action',action))
                after = replay.record(action,before,result=result)
                assert after == event['after'], ('trace drift after action',action)
                if before_blob is not None:
                    assert (after['floor'],after['x'],after['y']) == position, ('secret bumped instead of entered',position,after)
                    assert after['turns'] == before['turns']+1 and after['status'] == 16
                    for x1,y1,x2,y2 in hidden_lookup[position]:
                        for y in range(y1,y2+1):
                            for x in range(x1,x2+1):
                                if FLOORS[position[0]].rows[y][x] != '#':
                                    assert lib.tower_tile(replay.handle,position[0],x,y) != 1, ('room not fully revealed by entry',position,x,y)
                    seen_hidden.add(position)
                    count += 1
                if position == (13,3,4) and after['status'] == 17:
                    valve_snapshot = (export(replay.handle),action[1:])
                if position in quest_lookup:
                    q = quest_lookup[position]
                    if lib.tower_quest_state(replay.handle,q) == 2:
                        delivered.add(q)
                        # Re-enter the same NPC using the existing incoming step.
                        assert restore(scratch,export(replay.handle)) == 0
                        resources = tuple(after[k] for k in ('hp','attack','defence','yellow','blue','red','gold'))
                        lib.tower_move(scratch,-action[1],-action[2])
                        lib.tower_move(scratch,*action[1:])
                        assert lib.tower_quest_state(scratch,q) == 2
                        assert lib.lab_roundtrip(scratch) == 0
                        copied = Replay.__new__(Replay)
                        copied.lib,copied.handle = lib,scratch
                        assert tuple(copied.state()[k] for k in ('hp','attack','defence','yellow','blue','red','gold')) == resources, ('repeat NPC changes reward resources',q)
                        count += 1
                if action[0] == 'confirm' and after['status'] in (8,11):
                    location = (after['floor'],after['x'],after['y'])
                    if location in boss_locations:
                        seen_bosses.add(location)
                if after['floor'] == 40 and not preview_checked:
                    preview = replay.preview(15)  # Authored optional sentinel.
                    assert preview['defence'] >= after['attack'] and not preview['winnable']
                    assert preview['damage'] == 0xffffffff and preview['rounds'] == 0
                    count += 1
                    preview_checked = True
            final = export(replay.handle)
            assert replay.state() == trace['final']
            # Removing each mouth from an otherwise valid looted-room save
            # leaves balanced resource arithmetic but must still be rejected.
            for floor,mouth,rects in HIDDEN:
                forged = bytearray(final)
                flag(forged,(floor,*mouth),False)
                rejected_atomically(forged)
            for q,(npc,item,gate) in enumerate(QUESTS):
                assert lib.tower_quest_state(replay.handle,q) == 2
                for missing in (item,npc):
                    forged = bytearray(final)
                    flag(forged,missing,False)
                    rejected_atomically(forged)
            for mark in ((11,4,3),(26,5,6),(34,11,9)):
                forged = bytearray(final)
                flag(forged,mark,False)
                rejected_atomically(forged)
            assert valve_snapshot is not None
            blob,direction = valve_snapshot
            assert restore(scratch,blob) == 0
            for expected in (0,1,0):
                lib.tower_move(scratch,-direction[0],-direction[1])
                lib.tower_move(scratch,*direction)
                assert lib.tower_sluice_south(scratch) == expected
                assert lib.tower_tile(scratch,14,5,2) == (1 if expected else 0)
                assert lib.tower_tile(scratch,14,5,10) == (0 if expected else 1)
                assert lib.lab_roundtrip(scratch) == 0, 'repeated valve state does not save while standing on it'
                count += 1
            assert delivered == {0,1,2}, ('quest coverage',delivered)
            assert seen_bosses == boss_locations,seen_bosses
            assert len(seen_hidden) == 8, ('hidden entry coverage',seen_hidden)
            assert preview_checked
        finally:
            lib.tower_delete(scratch)
    print(f'PASS: {count} actual-C campaign contracts; trace reproduced; masked rooms, single-step entry, repeated valve save, quests, marks, negative-import atomicity and impossible late preview')
    print('LIMIT: model/save checks only; hidden-room UI rendering is not integrated or validated by this script.')


if __name__ == '__main__':
    main()
