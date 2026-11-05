# SPDX-License-Identifier: GPL-3.0-only
"""Offline current-music validation and restore-planning regressions; no MIDI."""
import unittest
import jam_dev as jd


def hashed(b):
    value = 0x811C9DC5
    for byte in b[:-4]:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    b[-4:] = value.to_bytes(4, 'little')
    return bytes(b)


def music(nb=0, nx=28):
    b = bytearray(jd.MAX_OBJ)
    b[:4] = (0x46554E42).to_bytes(4, 'little')
    b[4:8] = jd.MAX_OBJ.to_bytes(4, 'little')
    b[63:68] = bytes([2, 2, 0, 127, 8])
    pos = 68
    offsets = []
    for _ in range(2):
        b[pos:pos + 127] = bytes([64]) * 127
        pos += 127
        b[pos] = nx
        pos += 1
        b[pos:pos + nx] = bytes([64]) * nx
        pos += nx
        offsets.append(pos)
        b[pos] = nb
        pos += 1 + nb + 2 + 64 * 9
    return hashed(b), offsets, pos


class MusicRevision8(unittest.TestCase):
    def test_minimal_and_variable_extensions(self):
        for nx in (0, 28, 255):
            for nb in (0, 60, 198):
                with self.subTest(nx=nx, nb=nb):
                    self.assertIsNone(jd.check_object(0, music(nb, nx)[0]))

    def test_header_and_hash_damage(self):
        original = music()[0]
        for at, value in ((0, 0), (4, 1), (63, 4), (64, 0), (66, 126), (67, 9), (100, 65)):
            with self.subTest(at=at):
                b = bytearray(original); b[at] = value
                self.assertIsNotNone(jd.check_object(0, b))
        for b in (original[:-1], original + b'\0'):
            self.assertIsNotNone(jd.check_object(0, b))

    def test_malformed_records_even_with_valid_hash(self):
        original, offsets, motion = music()
        # Parameter, extension, End-B length, engine, note count, time, chance,
        # motion count and active-part flags are independent format boundaries.
        mutations = [(68, 192), (196, 192), (offsets[0], 199),
                     (offsets[0] + 1, jd.NENGINES),
                     (offsets[0] + 7, 5), (offsets[0] + 7, 24),
                     (offsets[0] + 11, 102), (motion, 129), (motion + 1, 4)]
        for at, value in mutations:
            with self.subTest(at=at, value=value):
                b = bytearray(original); b[at] = value
                self.assertIsNotNone(jd.check_object(0, hashed(b)))

    def test_revision7_contract_preserved(self):
        b = bytearray(music()[0]); b[67] = 7
        self.assertIsNone(jd.check_object(0, b))

    def test_default_restore_includes_valid_music_without_raw_override(self):
        b = music()[0]
        self.assertIsNone(jd.check_object(0, b))
        archive = jd.Archive(5, 'DUELL 1.1', 'test', [], {0: jd.Obj(0, b)}, [], [], False, 0)
        plan = jd.plan_restore(archive, {0: jd.Listed(len(b), jd.crc32(b))}, 0, sel={'music'})
        self.assertEqual([step.key for step in plan.steps], [0])
        self.assertEqual(plan.excluded, [])
        archive.objects[0].raw = True
        raw_plan = jd.plan_restore(archive, {0: jd.Listed(len(b), jd.crc32(b))}, 0, sel={'music'})
        self.assertEqual(raw_plan.steps, [])
        self.assertEqual(len(raw_plan.excluded), 1)


if __name__ == '__main__':
    unittest.main()
