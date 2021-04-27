import unittest

from _proton_core import lib


class MessageTests(unittest.TestCase):
    def test_durable_property(self):
        m = lib.pn_message()
        self.assertFalse(lib.pn_message_is_durable(m))
        lib.pn_message_set_durable(m, True)
        self.assertTrue(lib.pn_message_is_durable(m))
        lib.pn_message_free(m)

    def test_priority_property(self):
        m = lib.pn_message()
        self.assertEqual(4, lib.pn_message_get_priority(m))
        lib.pn_message_set_priority(m, 42)
        self.assertEqual(42, lib.pn_message_get_priority(m))
        lib.pn_message_free(m)


if __name__ == '__main__':
    exit(unittest.main())
