"""Static regression: the real hardware ACK probe must report actionable failures."""
from pathlib import Path
import re

src = Path("experiments/v2-probe/gamecube-ack.c").read_text()
assert "CDAP_FAIL_WATCHDOG" in src
assert "CDAP_FAIL_SI_CALLBACK" in src
assert "CDAP_FAIL_RETRY_LIMIT" in src
assert re.search(r"if\s*\(joybus_callback_result\s*!=\s*0\)[\s\S]{0,110}fail_probe\(CDAP_FAIL_SI_CALLBACK\)",src)
assert "fail_probe(CDAP_FAIL_RETRY_LIMIT);" in src
assert "fail_probe(CDAP_FAIL_WATCHDOG);" in src
assert "failure_stage = joybus_stage;" in src
assert "failure_callback_result = joybus_callback_result;" in src
assert "FAIL code %u at stage %d" in src
assert "SI result %08lX" in src
assert "last reply %08lX" in src
print("ACK experimental runtime failure snapshot diagnostics present.")
