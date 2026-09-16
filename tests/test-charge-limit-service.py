import importlib.machinery
import importlib.util
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "config/charge-limit/negroni-charge-limit"
loader = importlib.machinery.SourceFileLoader("charge_service", str(SCRIPT))
spec = importlib.util.spec_from_loader(loader.name, loader)
service = importlib.util.module_from_spec(spec)
loader.exec_module(service)

class FakeBattery:
    def __init__(self, start=75, end=80, enabled=0):
        self.values = dict(charge_control_start_threshold=str(start),
                           charge_control_end_threshold=str(end), charge_limit_enable=str(enabled),
                           charge_limit_status="enabled=1 state=holding error=0")
        self.writes = []
    def read(self, battery, name):
        return self.values[name]
    def write(self, battery, name, value):
        updated = dict(self.values, **{name: str(value)})
        if not int(updated["charge_control_start_threshold"]) < int(updated["charge_control_end_threshold"]):
            raise OSError("invalid transient interval")
        self.values = updated
        self.writes.append((name, value))

class ServiceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.config = Path(self.tmp.name) / "config"
    def test_configuration_and_malformed_input(self):
        self.config.write_text("START_THRESHOLD=075 # start\nEND_THRESHOLD=80\n")
        self.assertEqual(service.read_config(self.config), (75, 80))
        for contents in ("START_THRESHOLD=no", "START_THRESHOLD=-1", "END_THRESHOLD=101",
                         "START_THRESHOLD=80", "UNKNOWN=1", "START_THRESHOLD=70\nSTART_THRESHOLD=75"):
            with self.subTest(contents=contents):
                self.config.write_text(contents)
                with self.assertRaises(ValueError): service.read_config(self.config)
    def test_crossing_and_equal_old_threshold(self):
        for initial, target in (((75, 80), (80, 90)), ((75, 80), (85, 95)),
                                ((75, 80), (40, 60)), ((40, 60), (75, 80))):
            fake = FakeBattery(*initial)
            with patch.object(service, "read", fake.read), patch.object(service, "write", fake.write):
                service.apply(None, *target)
                self.assertEqual(fake.values["charge_limit_enable"], "1")
                before = len(fake.writes)
                service.apply(None, *target)
                self.assertEqual(len(fake.writes), before) # no repeated hardware command writes
    def test_firmware_reprobe_reapplies(self):
        fake = FakeBattery(75, 80, 1)
        with patch.object(service, "read", fake.read), patch.object(service, "write", fake.write):
            service.apply(None, 65, 70)
            fake.values.update(charge_control_start_threshold="75", charge_control_end_threshold="80", charge_limit_enable="0")
            service.apply(None, 65, 70)
            self.assertEqual(fake.values["charge_control_end_threshold"], "70")
            self.assertEqual(fake.writes[-1], ("charge_limit_enable", 1))
    def test_failed_driver_is_not_ready(self):
        fake = FakeBattery()
        fake.values["charge_limit_status"] = "enabled=1 state=pending error=-121"
        with patch.object(service, "read", fake.read), patch.object(service, "write", fake.write):
            with self.assertRaises(OSError): service.apply(None, 75, 80)
    def test_invalid_configuration_always_releases(self):
        fake = FakeBattery(enabled=1)
        self.config.write_text("END_THRESHOLD=wrong")
        with patch.object(service, "read", fake.read), patch.object(service, "write", fake.write):
            with self.assertRaises(ValueError): service.run(None, self.config, threading.Event())
        self.assertEqual(fake.writes, [("charge_limit_enable", 0)])
    def test_stop_releases_policy(self):
        fake = FakeBattery()
        self.config.write_text("START_THRESHOLD=75\nEND_THRESHOLD=80")
        stop = threading.Event()
        def notified(message):
            if message == "READY=1": stop.set()
        with patch.object(service, "read", fake.read), patch.object(service, "write", fake.write), patch.object(service, "notify", notified):
            service.run(None, self.config, stop, interval=0)
        self.assertEqual(fake.writes[-1], ("charge_limit_enable", 0))
    def test_startup_timeout_releases_policy(self):
        fake = FakeBattery(enabled=1)
        self.config.write_text("START_THRESHOLD=75\nEND_THRESHOLD=80")
        with patch.object(service, "apply", side_effect=OSError("ADSP unavailable")), patch.object(service, "read", fake.read), patch.object(service, "write", fake.write):
            with self.assertRaises(RuntimeError): service.run(None, self.config, threading.Event(), interval=0, startup_timeout=0)
        self.assertEqual(fake.writes[-1], ("charge_limit_enable", 0))
    def test_absent_kernel_interface_is_not_created(self):
        with self.assertRaises(FileNotFoundError): service.write(Path(self.tmp.name), "charge_limit_enable", 1)
        self.assertFalse((Path(self.tmp.name) / "charge_limit_enable").exists())
        service.disable(Path(self.tmp.name))

if __name__ == "__main__": unittest.main()
