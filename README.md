# Localizzazione relativa con ESP32 e Bluetooth Low Energy

Manuale per riprodurre il sistema descritto nella tesi *Studio e sviluppo di soluzioni embedded per la rilevazione dinamica di posizioni in ambienti indoor tramite tecnologia BLE* (Daniele Di Croce, Università degli Studi della Basilicata, A.A. 2025/2026).

Tre schede ESP32 si localizzano reciprocamente misurando l'RSSI dei pacchetti BLE che si scambiano. Uno smartphone Android può essere localizzato come quarto elemento. Le posizioni vengono stampate in formato JSON sulla porta seriale e disegnate da uno script Python.

## 1. Materiale necessario

- 3 schede ESP32-WROOM-32 (stesso modello per tutte e tre).
- 3 cavi USB per la programmazione e l'alimentazione delle schede.
- 1 smartphone Android con l'app *nRF Connect for Mobile* (Nordic Semiconductor).
- 1 calcolatore, collegato a una delle tre schede (nelle prove descritte nella tesi, il Nodo 0).

**Nei test è stato utilizzato un calcolatore con Sistema Operativo Arch Linux, pertanto le informazioni disponibili dedicate ad altri Sistemi Operativi sono state ricercate in rete, ma non sono state testate.**

## 2. Struttura del repository

```text
{repo}/
├── firmware/
│   └── esp32_rel_loc/
│       └── esp32_rel_loc.ino    # Sketch Arduino (uguale per le tre schede)
└── python/
    └── visualize_positions.py   # Script di visualizzazione
```

La cartella dello sketch deve avere lo stesso nome del file `.ino`, come richiesto da Arduino IDE.

## 3. Installazione del software

Le versioni indicate sono quelle con cui il sistema è stato sviluppato e provato. Non sono prescrittive: versioni diverse possono funzionare, ma non sono state verificate.

| Componente | Versione usata nelle prove |
|---|---|
| Arduino IDE | 2.3.10 |
| Pacchetto schede *esp32 by Espressif Systems* | 3.3.12 |
| Python | 3.14.7 |
| `matplotlib` | 3.11.2 |
| `pyserial` | 3.5 |
| Sistema operativo | Arch Linux |

### 3.1 Arduino IDE e supporto alle schede ESP32

1. Installare Arduino IDE dal sito ufficiale: https://www.arduino.cc/en/software
2. Aprire *Strumenti > Scheda > Gestore schede*. Non serve aggiungere nessun indirizzo nelle preferenze: il pacchetto si trova direttamente nel Gestore schede.
3. Cercare **esp32** e installare **esp32 by Espressif Systems**. Nel Gestore schede la versione si può scegliere dal menu a tendina nella scheda del pacchetto.
4. La libreria `BLEDevice` è inclusa nel core: non servono librerie esterne.

### 3.2 Python e librerie per la visualizzazione

1. Installare Python 3.
2. Creare un ambiente virtuale e installare le librerie:
   ```bash
   cd python
   python3 -m venv .venv
   source .venv/bin/activate
   pip install pyserial matplotlib
   ```

#### Solo per utenti Linux

3. Installare il supporto grafico Tkinter, necessario perché `matplotlib` possa aprire la finestra del grafico:
   - Arch Linux: `sudo pacman -S tk`
   - Debian e Ubuntu: `sudo apt install python3-tk` (non testato)
4. Accesso alla porta seriale (gruppo `dialout`):
   - Arch Linux: non è necessario aggiungere l'utente al gruppo `dialout`.
   - Debian e Ubuntu: il sistema non è stato testato su queste distribuzioni, quindi potrebbe essere necessario aggiungere l'utente al gruppo `dialout` (`sudo usermod -aG dialout $USER`, poi riavviare la sessione).

## 4. Preparazione e caricamento del firmware

Le tre schede eseguono lo stesso sketch. Cambia soltanto la costante `NODE_ID`.

1. Aprire in Arduino IDE lo sketch `firmware/esp32_rel_loc/esp32_rel_loc.ino`.
2. Selezionare la scheda in *Strumenti > Scheda > esp32* e scegliere **ESP32 Dev Module**. Tutte le altre impostazioni del menu *Strumenti* restano ai valori predefiniti.
3. Per la **prima scheda** impostare `#define NODE_ID 0`, collegarla al computer, selezionare la porta in *Strumenti > Porta* e caricare lo sketch.
4. Ripetere con `#define NODE_ID 1` per la seconda scheda e con `#define NODE_ID 2` per la terza.
5. **Controllo importante**: ogni scheda deve avere un Node ID diverso. Con due schede uguali il sistema non segnala errori ma mescola le loro misure.

I parametri del modello si trovano all'inizio dello sketch:

| Costante | Valore usato | Significato |
|---|---|---|
| `CALIBRATION_MODE` | false | `true` stampa gli RSSI grezzi dei nodi fissi per la calibrazione e disattiva la stampa JSON |
| `RSSI_AT_1M` | -56.0 | RSSI misurato a 1 m (da ricalibrare per ogni ambiente) |
| `PATH_LOSS_EXPONENT` | 3.0 | esponente di path loss $n$ (da ricalibrare) |
| `PROCESS_NOISE` | 0.5 | rumore di processo $Q$ del filtro di Kalman |
| `MEASUREMENT_NOISE` | 10.0 | rumore di misura $R$ (da ricalibrare) |

## 5. Calibrazione di RSSI a 1 m e path loss exponent

I valori della tabella valgono per l'ambiente della tesi. Cambiando ambiente vanno ricavati di nuovo.

1. Impostare `#define CALIBRATION_MODE true` all'inizio dello sketch e ricaricarlo sulla scheda collegata al computer. In questa modalità la scheda stampa sulla porta seriale gli RSSI grezzi dei pacchetti ricevuti dagli altri nodi fissi, e la stampa JSON delle posizioni è disattivata, quindi lo script Python non riceve dati finché la modalità resta attiva.
   Aprire il Serial Monitor di Arduino IDE a **115200 baud**: ogni pacchetto ricevuto da un altro nodo fisso produce una riga del tipo (valori di esempio):
   ```
   [Node 1] Raw RSSI: -56 | Filtered RSSI: -56 -> Dist: 1.00
   ```
2. Posizionare due schede a 1 m di distanza, in linea di vista diretta e senza ostacoli. Per evitare errori e ridurre al minimo le variabili incontrollate, posizionare entrambe le schede alla stessa altezza da terra; l'orientamento reciproco è invece secondario, essendo le antenne PCB dell'ESP32-WROOM-32 quasi-omnidirezionali. Raccogliere infine almeno 10 campioni e calcolarne la media aritmetica, assumendo una distribuzione gaussiana degli errori. 
3. Calcolare la media dei valori `Raw RSSI` raccolti: è il valore di `RSSI_AT_1M`.
4. Ripetere la misura a una seconda distanza nota $d_1$ (nella tesi 5 m, con RSSI medio di -77 dBm) e ricavare l'esponente con la formula
   $n = \dfrac{RSSI(d_0) - RSSI(d_1)}{10 \log_{10}(d_1 / d_0)}$ dove $d_0 = 1$ m.
5. Aggiornare le due costanti nello sketch, riportare `CALIBRATION_MODE` a `false` e ricaricarlo su **tutte e tre** le schede.

## 6. Configurazione dello smartphone come dispositivo esterno (facoltativa)

Con l'app *nRF Connect for Mobile* lo smartphone viene configurato come *Broadcaster* puro.

1. Aprire l'applicazione **nRF Connect for Mobile**.
2. Aprire il menu laterale a scomparsa (icona ad hamburger in alto a sinistra oppure swipe dal bordo sinistro) e selezionare la voce **Devices**.
3. Nella barra di navigazione superiore, selezionare la scheda **ADVERTISER**.
4. Premere il pulsante floating **`+`** (in basso a destra) per creare un nuovo pacchetto di trasmissione (Advertising packet).
5. Configurare il pacchetto (impostando il nome dispositivo desiderato e i dati broadcast) e attivare l'interruttore accanto al profilo creato per avviare la trasmissione continua.

Impostazioni che devono risultare (dalla tesi, Capitolo 3):

- Manufacturer Specific Data con Company ID `0xFFFF` e identificativo `0x03` (corrispondente a `EXTERNAL_ID 3`).
    1. Premere **Add record** sotto la sezione *Advertising data*.
    2. Selezionare la voce **Manufacturer Data**.
    3. Nel campo **Company Identifier** inserire `0xFFFF`.
    4. Nel campo **Data** inserire `03` (corrispondente a `EXTERNAL_ID 3`).
    5. Salvare premendo *OK*.
- pacchetto non connettibile e non scansionabile;
- **opzione "Adv. Extension" disattivata** (PHY primario e secondario su Legacy): con l'Extended Advertising le schede non vedono il telefono;
- intervallo di advertising di circa 250 ms.

## 7. Disposizione dei nodi e avvio

1. Posizionare le tre schede alla stessa altezza da terra, in linea di vista diretta e prive di ostacoli interposti. I nodi devono formare una disposizione geometrica non allineata (triangolare), requisito necessario per consentire la corretta trilaterazione 2D del target ed evitare ambiguità geometriche. La distanza reciproca tra le schede deve rientrare nel range operativo affidabile del BLE (consigliato entro 5–10 m; oltre tale soglia l'attenuazione del segnale e il rumore degradano sensibilmente la stima RSSI).
2. Alimentare le tre schede (l'ordine di accensione è indifferente; i nodi avviano autonomamente le routine di advertising e ricezione).
3. Collegare al computer tramite cavo USB la scheda del Nodo 0 e identificare la porta seriale assegnata:
   - **Linux:** verificare i device disponibili eseguendo:
     ```bash
     ls /dev/ttyUSB* /dev/ttyACM*
     ```
     *(solitamente individuata come `/dev/ttyUSB0` o `/dev/ttyACM0`)*
   - **Windows:** aprire **Gestione dispositivi** e consultare la sezione **Porte (COM e LPT)** per individuare l'identificativo assegnato (es. `COM3` o `COM4`).
4. Aprire `python/visualize_positions.py` e verificare che la variabile `SERIAL_PORT` corrisponda alla porta identificata al punto precedente.
5. Avviare lo script all'interno dell'ambiente virtuale:
   - **Linux / macOS:**
     ```bash
     source .venv/bin/activate
     python visualize_positions.py
     ```
   - **Windows:**
     ```cmd
     .venv\Scripts\activate
     python visualize_positions.py
     ```
6. Attendere l'apertura dell'interfaccia grafica: i primi campioni e la posizione stimata dei nodi compaiono a schermo entro qualche secondo dall'avvio della lettura seriale.

## 8. Cosa aspettarsi

Ogni 3 secondi la scheda stampa una riga JSON, ad esempio:

```json
{"n":[{"i":0,"x":0.00,"y":0.00},{"i":1,"x":1.20,"y":0.00},{"i":2,"x":0.65,"y":0.50}],"d":{"x":0.30,"y":-0.40,"r":1.25}}
```

- `n`: i tre nodi fissi con le loro coordinate in metri.
- `d`: il dispositivo esterno, con le coordinate `x` e `y` e la distanza `r`.

Il grafico mostra:

- i nodi 0, 1 e 2 rispettivamente in blu, rosso e verde, con le coordinate in alto a sinistra;
- il dispositivo esterno come una X magenta, visibile solo quando è stato localizzato;
- la X grigia con etichetta "Last position" se la distanza dal dispositivo resta uguale per 5 letture consecutive, cioè se il dispositivo ha smesso di trasmettere.

Il Nodo 0 è sempre nell'origine, il Nodo 1 sull'asse delle ascisse e il Nodo 2 con ordinata positiva. Le coordinate cambiano leggermente da una scheda all'altra, perché ognuna le calcola con le proprie misure.

## 9. Problemi comuni

| Problema | Causa e soluzione |
|---|---|
| Node ID privi di senso sul monitor seriale | Manca il controllo sul Company ID oppure il pacchetto non è del sistema (il codice finale lo include) |
| Caratteri illeggibili dopo il caricamento | Porta seriale in stato anomalo: scollegare e ricollegare il cavo USB, premere il pulsante di reset della scheda oppure cancellare la cache del Serial Monitor di Arduino IDE con l'apposito tasto software |
| Il telefono non viene mai rilevato | Disattivare "Adv. Extension" in nRF Connect (solo advertising Legacy) |
| Il grafico non compare | Installare il supporto a Tkinter |
| Il grafico resta vuoto e il monitor seriale mostra righe `Raw RSSI` | `CALIBRATION_MODE` è `true`: impostarlo a `false` e ricaricare lo sketch |
| La finestra si apre ma è vuota | Righe JSON incomplete o porta occupata da un altro programma: chiudere il Serial Monitor di Arduino IDE e riavviare lo script |
| Il nodo risulta in una posizione senza senso | Due schede con lo stesso `NODE_ID` oppure RSSI molto instabile (limite dell'utilizzo del solo RSSI) |

Per l'analisi completa dei problemi incontrati si veda il Capitolo 3 della tesi.
