"""Compila gli sketch in esp32_* come librerie per PC (serve un compilatore C++: g++/MinGW/clang)."""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(HERE, "build")

# Cartella che contiene le tre cartelle degli sketch (esp32_anchor, ...). Si puo' forzare con la variabile
# d'ambiente SKETCH_DIR oppure scrivendo qui il percorso (assoluto, oppure relativo alla cartella sim,
# per esempio "../../firmware"); altrimenti viene cercata accanto alla cartella
# di questo progetto (../firmware, ../../firmware oppure la cartella stessa).
SKETCH_DIR = os.environ.get("SKETCH_DIR", "")


# Nomi possibili delle cartelle/sketch (la cartella e il file .ino hanno lo stesso nome). Se i tuoi si chiamano
# in un altro modo, aggiungi il nome alla lista giusta.
SKETCH_NAMES = {
    "anchor": ["esp32_anchor"],
    "broadcast_observer": ["esp32_robot_broadcast_observer"],
    "broadcast": ["esp32_robot_broadcast"],
}


def find_sketch(folder, kind):
    for name in SKETCH_NAMES[kind]:
        path = os.path.join(folder, name, name + ".ino")
        if os.path.exists(path):
            return path
    return None


def find_sketch_dir():
    # un percorso relativo e' inteso rispetto alla cartella sim (dove si trova questo file)
    candidates = [os.path.normpath(os.path.join(HERE, SKETCH_DIR))] if SKETCH_DIR else [
        os.path.join(ROOT, "firmware"),
        os.path.join(os.path.dirname(ROOT), "firmware"),
        ROOT,
    ]
    for folder in candidates:
        if all(find_sketch(folder, kind) for kind in SKETCH_NAMES):
            return folder
    raise RuntimeError("Sketch non trovati. Cercati in: " + ", ".join(candidates) + ". Servono le cartelle "
                       + ", ".join(names[0] for names in SKETCH_NAMES.values())
                       + " (ognuna con il file .ino omonimo). Imposta SKETCH_DIR in sim/build.py.")


FIRMWARE = find_sketch_dir()

SKETCHES = {kind: find_sketch(FIRMWARE, kind) for kind in SKETCH_NAMES}

LIB_EXT = ".dll" if os.name == "nt" else ".dylib" if sys.platform == "darwin" else ".so"

WRAPPER_TAIL = r"""
#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif
EXPORT void sim_start(int id) { g_node_id = id; setup(); }
EXPORT void sim_loop() { loop(); }
EXPORT void sim_rx(const char *data, int length, int rssi) {
  BLEAdvertisedDevice device;
  device.manufacturerData = std::string(data, length);
  device.rssi = rssi;
  BLEScan *scan = BLEDevice::getScan();
  if (scan->callbacks) scan->callbacks->onResult(device);
}
EXPORT int sim_payload(char *buffer, int capacity) {
  const std::string &p = BLEDevice::getAdvertising()->current;
  int n = (int)p.size() < capacity ? (int)p.size() : capacity;
  memcpy(buffer, p.data(), n);
  return n;
}
EXPORT int sim_serial(char *buffer, int capacity) {
  int n = (int)Serial.out.size() < capacity - 1 ? (int)Serial.out.size() : capacity - 1;
  memcpy(buffer, Serial.out.data(), n);
  buffer[n] = 0;
  Serial.out.clear();
  return n;
}
"""


def wrapper_source(sketch_path):
    text = open(sketch_path, encoding="utf-8").read()
    text = re.sub(r"^#include\s*<.*?>\s*$", "", text, flags=re.M)
    
    # l'ID della scheda diventa una variabile, cosi' una sola libreria serve per tutte le schede
    text, count = re.subn(r"^#define\s+NODE_ID\s+\d+", "#define NODE_ID g_node_id", text, flags=re.M)
    if count != 1:
        raise RuntimeError(f"{sketch_path}: manca la riga '#define NODE_ID <numero>'")
    return '#include "../arduino_stub.h"\nstatic int g_node_id = 0;\n' + text + "\n" + WRAPPER_TAIL


def library_path(kind):
    return os.path.join(BUILD, kind + LIB_EXT)


def needs_build(kind):
    lib = library_path(kind)
    if not os.path.exists(lib):
        return True
    newest = max(os.path.getmtime(SKETCHES[kind]), os.path.getmtime(os.path.join(HERE, "arduino_stub.h")),
                 os.path.getmtime(__file__))
    return os.path.getmtime(lib) < newest


def build(kind):
    os.makedirs(BUILD, exist_ok=True)
    source = os.path.join(BUILD, kind + ".cpp")
    with open(source, "w", encoding="utf-8") as f:
        f.write(wrapper_source(SKETCHES[kind]))
    compiler = os.environ.get("CXX", "g++")
    command = [compiler, "-std=c++17", "-O1", "-shared", "-fPIC", "-fvisibility=hidden", "-o", library_path(kind), source]
    try:
        result = subprocess.run(command, capture_output=True, text=True)
    except FileNotFoundError:
        raise RuntimeError(f"Compilatore '{compiler}' non trovato: installa g++ (o imposta la variabile CXX)")
    if result.returncode != 0:
        raise RuntimeError(f"Compilazione di {kind} fallita:\n{result.stderr}")


def build_all(kinds=("anchor", "broadcast_observer", "broadcast")):
    for kind in kinds:
        if needs_build(kind):
            print(f"[build] compilo {kind}")
            build(kind)


_instance_counter = 0
_INSTANCE_FOLDER = os.path.join(BUILD, "instances")


def clean_old_instances():
    """Cancella le copie lasciate da esecuzioni precedenti (se non sono in uso)."""
    if os.path.isdir(_INSTANCE_FOLDER):
        for name in os.listdir(_INSTANCE_FOLDER):
            try:
                os.remove(os.path.join(_INSTANCE_FOLDER, name))
            except OSError:
                pass


def instance_library(kind, index):
    """Ogni scheda ha la propria copia della libreria, con un nome mai usato prima:
    cosi' lo stato globale dello sketch non e' condiviso e una copia gia' caricata non viene sovrascritta."""
    global _instance_counter
    if _instance_counter == 0:
        clean_old_instances()
    _instance_counter += 1
    os.makedirs(_INSTANCE_FOLDER, exist_ok=True)
    path = os.path.join(_INSTANCE_FOLDER, f"{os.getpid()}_{_instance_counter}_{kind}_{index}{LIB_EXT}")
    shutil.copyfile(library_path(kind), path)
    return path


if __name__ == "__main__":
    build_all()