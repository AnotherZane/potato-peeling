"""timeout=, cancel=, Ctrl-C and the GIL. SLOW is a polygon whose exact solve takes well over
20 s, so a stop that does not work shows up as a long hang, not a pass."""
import _thread
import os
import signal
import subprocess
import sys
import threading
import time

import pytest

import potato_peeling as pp

from conftest import SLOW, assert_certified

LATE = 1.0  # seconds a stop may take to act (the library checks every ~20 ms)


@pytest.mark.parametrize("method", [
    pp.exact_convex_skull,
    lambda p, timeout: pp.upper_bound(p, rel_tol=0, timeout=timeout),
])
def test_timeout_raises(method):
    t = time.monotonic()
    with pytest.raises(TimeoutError):
        method(SLOW, timeout=0.3)
    assert time.monotonic() - t < 0.3 + LATE


def test_refine_timeout_returns_answer_so_far():
    start = [(0, 0), (1, 0), (0, 1)]
    t = time.monotonic()
    s = pp.refine(SLOW, start, max_sweeps=10_000, adjoin_rounds=50, timeout=0.2)
    assert time.monotonic() - t < 0.2 + LATE
    assert_certified(SLOW, s)
    assert s.area >= pp.refine(SLOW, start, max_sweeps=0).area


def test_convex_skull_timeout_returns_or_raises():
    # before refinement starts it raises; once refining it returns the answer so far
    for timeout in (0.001, 0.5):
        t = time.monotonic()
        try:
            assert_certified(SLOW, pp.convex_skull(SLOW, timeout=timeout))
        except TimeoutError:
            pass
        assert time.monotonic() - t < timeout + LATE


def test_generous_timeout_changes_nothing():
    p = [(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)]
    assert pp.exact_convex_skull(p, timeout=60) == pp.exact_convex_skull(p)


def _cancel_after(seconds):
    """An Event set from another thread, as a cancel button would (the solve
    itself runs in this thread)."""
    event = threading.Event()
    timer = threading.Timer(seconds, event.set)
    timer.start()
    return event, timer


@pytest.mark.parametrize("method", [
    pp.exact_convex_skull,
    lambda p, cancel: pp.upper_bound(p, rel_tol=0, cancel=cancel),
])
def test_cancel_raises_cancelled(method):
    cancel, timer = _cancel_after(0.3)
    t = time.monotonic()
    try:
        with pytest.raises(pp.Cancelled):
            method(SLOW, cancel=cancel)
    finally:
        timer.cancel()
    assert time.monotonic() - t < 0.3 + LATE


def test_cancel_in_a_worker_thread():
    # The QGIS case: the solve runs in a worker thread, cancelled from the main one
    # through a callable (QGIS passes feedback.isCanceled).
    flag = threading.Event()
    outcome = []

    def solve():
        try:
            pp.exact_convex_skull(SLOW, cancel=flag.is_set)
        except pp.Cancelled:
            outcome.append(time.monotonic())

    worker = threading.Thread(target=solve)
    worker.start()
    time.sleep(0.3)
    t = time.monotonic()
    flag.set()
    worker.join(timeout=10)
    assert not worker.is_alive()
    assert outcome and outcome[0] - t < LATE


def test_refine_cancel_returns_answer_so_far():
    start = [(0, 0), (1, 0), (0, 1)]
    cancel, timer = _cancel_after(0.2)
    t = time.monotonic()
    try:
        s = pp.refine(SLOW, start, max_sweeps=10_000, adjoin_rounds=50, cancel=cancel)
    finally:
        timer.cancel()
    assert time.monotonic() - t < 0.2 + LATE
    assert_certified(SLOW, s)
    assert s.area >= pp.refine(SLOW, start, max_sweeps=0).area


def test_cancel_true_from_the_start():
    # Any truthy value stops; convex_skull raises before refining, returns after.
    t = time.monotonic()
    with pytest.raises(pp.Cancelled):
        pp.exact_convex_skull(SLOW, cancel=lambda: 1)
    try:
        assert_certified(SLOW, pp.convex_skull(SLOW, cancel=lambda: True))
    except pp.Cancelled:
        pass
    assert time.monotonic() - t < 2 * LATE


def test_cancel_exception_propagates():
    class Boom(Exception):
        pass

    def cancel():
        raise Boom

    t = time.monotonic()
    with pytest.raises(Boom):
        pp.exact_convex_skull(SLOW, cancel=cancel)
    assert time.monotonic() - t < LATE


def test_unset_cancel_changes_nothing():
    p = [(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)]
    assert pp.exact_convex_skull(p, cancel=threading.Event()) == pp.exact_convex_skull(p)
    assert pp.convex_skull(p, cancel=lambda: False, timeout=60) == pp.convex_skull(p)


def test_ctrl_c():
    timer = threading.Timer(0.3, _thread.interrupt_main)
    t = time.monotonic()
    timer.start()
    try:
        with pytest.raises(KeyboardInterrupt):
            pp.exact_convex_skull(SLOW, timeout=30)
    finally:
        timer.cancel()
    assert time.monotonic() - t < 0.3 + LATE


@pytest.mark.skipif(os.name != "posix", reason="sends a real SIGINT")
def test_sigint_in_a_subprocess():
    code = ("import sys, potato_peeling as pp\n"
            f"p = {SLOW!r}\n"
            "print('ready', flush=True)\n"
            "pp.exact_convex_skull(p, timeout=30)\n")
    proc = subprocess.Popen([sys.executable, "-c", code], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    try:
        assert proc.stdout.readline().strip() == "ready"
        time.sleep(0.3)
        t = time.monotonic()
        proc.send_signal(signal.SIGINT)
        _, err = proc.communicate(timeout=10)
        assert time.monotonic() - t < LATE
    finally:
        proc.kill()
    assert proc.returncode != 0
    assert "KeyboardInterrupt" in err


def test_gil_released():
    # While a 2 s solve runs in another thread, the main thread keeps running
    # Python: about 100-200 10 ms sleeps even on a slow runner. Were the GIL held,
    # it would be stuck for the whole solve and count 0 or 1.
    def solve():
        try:
            pp.exact_convex_skull(SLOW, timeout=2.0)
        except TimeoutError:
            pass

    worker = threading.Thread(target=solve)
    worker.start()
    ticks = 0
    while worker.is_alive():
        ticks += 1
        time.sleep(0.01)
    worker.join()
    assert ticks >= 20


def test_parallel_calls_agree():
    from concurrent.futures import ThreadPoolExecutor
    p = [(0, 0), (7, 0), (7, 3), (6, 3), (6, 1), (5, 1), (5, 3), (4, 3), (4, 1), (3, 1),
         (3, 3), (2, 3), (2, 1), (1, 1), (1, 3), (0, 3)]
    expected = pp.approximate_convex_skull(p, threads=1)
    with ThreadPoolExecutor(4) as ex:
        results = list(ex.map(lambda _: pp.approximate_convex_skull(p, threads=1), range(8)))
    assert all(r == expected for r in results)
