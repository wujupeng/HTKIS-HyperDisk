"""HTKIS HyperDisk - Boot Test Monitor
Polls Gateway /boot/stats and /boot/machines every 5s.
Shows live success rate, duration stats, and per-boot history.
"""
import requests, time, json, os, sys

GATEWAY = "http://192.168.2.80:8080/api/v1/boot"
TARGET = 50

def clear():
    os.system('cls' if os.name == 'nt' else 'clear')

def poll():
    try:
        stats = requests.get(f"{GATEWAY}/stats", timeout=3).json()
        machines = requests.get(f"{GATEWAY}/machines", timeout=3).json()
        sessions = requests.get(f"{GATEWAY}/sessions", timeout=3).json()
        return stats, machines, sessions
    except:
        return None, None, None

def render(stats, machines, sessions):
    clear()
    print("=" * 60)
    print("  HTKIS HyperDisk X - Boot Test Monitor")
    print("=" * 60)
    if not stats:
        print("  [ERROR] Cannot reach Gateway at 192.168.2.80:8080")
        return

    boots = stats["boots"]
    success = stats["success"]
    failed = stats["failed"]
    rate = (success / boots * 100) if boots > 0 else 0
    progress = min(boots, TARGET)

    print(f"\n  Target: {TARGET} consecutive boots")
    print(f"  Progress: [{'#' * progress}{'.' * (TARGET - progress)}] {boots}/{TARGET}")
    print()
    print(f"  Total Boots:  {boots}")
    print(f"  Success:      {success}")
    print(f"  Failed:       {failed}")
    print(f"  Success Rate: {rate:.1f}%")
    print()
    print(f"  Avg Duration: {stats['avg_duration_ms']}ms")
    print(f"  Min Duration: {stats['min_duration_ms']}ms")
    print(f"  Max Duration: {stats['max_duration_ms']}ms")
    print()

    if machines:
        print("  Machines:")
        for m in machines:
            mrate = (m["success"] / m["boots"] * 100) if m["boots"] > 0 else 0
            print(f"    {m['machine_id']}  boots={m['boots']}  ok={m['success']}  fail={m['failed']}  rate={mrate:.0f}%  last={m.get('last_seen','')[:19]}")

    print()

    if sessions and sessions.get("sessions"):
        recent = sessions["sessions"][-5:]
        print("  Last 5 sessions:")
        for s in reversed(recent):
            mark = "OK" if s["result"] == "success" else "FAIL"
            print(f"    [{mark}] boot_id={s['boot_id'][:8]}... phase={s['phase']} dur={s['duration_ms']}ms")

    print()
    if boots >= TARGET and rate >= 98:
        print("  >>> TARGET ACHIEVED! >= 98% success over {} boots <<<".format(TARGET))
    elif boots >= TARGET:
        print(f"  >>> {TARGET} boots reached but rate {rate:.1f}% < 98% <<<")

    print("\n  Press Ctrl+C to exit")

def main():
    print("Connecting to Gateway...")
    while True:
        stats, machines, sessions = poll()
        render(stats, machines, sessions)
        time.sleep(5)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nMonitor stopped.")
