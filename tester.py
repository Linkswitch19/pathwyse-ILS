import subprocess
import re
import statistics
import csv
import os

# --- 1. CONFIGURAZIONI ---
combinazioni = [
    {"id": 1, "t": 0.1, "td": 1,  "k": 10,  "kd": 1},
    {"id": 2, "t": 0.1, "td": 10, "k": 100, "kd": 10},
    {"id": 3, "t": 1.0, "td": 10, "k": 100, "kd": 50},
    {"id": 4, "t": 1.0, "td": 10, "k": 500, "kd": 50},
]

seeds = [12345, 42, 100, 999, 2026]

istanze = [
    "instances\\A-n54-k7-149.sppcc",
    "instances\\A-n60-k9-57.sppcc",
    "instances\\A-n80-k10-14.sppcc",
    "instances\\B-n45-k6-54.sppcc",
    "instances\\B-n78-k10-70.sppcc",
    "instances\\M-n101-k10-97.sppcc",
    "instances\\M-n151-k12-15.sppcc",
    "instances\\M-n200-k17-12.sppcc",
    "instances\\P-n76-k4-41.sppcc",
    "instances\\P-n76-k5-16.sppcc",
    "instances\\P-n101-k4-174.sppcc"
]

pw_defaults = {
    "instances\\A-n54-k7-149.sppcc": (-12492.0, 9.234485),
    "instances\\A-n60-k9-57.sppcc": (-1000.0, 27.294795),
    "instances\\A-n80-k10-14.sppcc": (-105283.0, 65.785667),
    "instances\\B-n45-k6-54.sppcc": (-74278.0, 96.116750),
    "instances\\B-n78-k10-70.sppcc": (-44333.0, 34.470044),
    "instances\\M-n101-k10-97.sppcc": (-32628.0, 60.905079),
    "instances\\M-n151-k12-15.sppcc": (-79996.0, 1040.815193),
    "instances\\M-n200-k17-12.sppcc": (-121210.0, 712.354138),
    "instances\\P-n76-k4-41.sppcc": (-88276.0, 106.791573),
    "instances\\P-n76-k5-16.sppcc": (-107633.0, 39.989066),
    "instances\\P-n101-k4-174.sppcc": (-17702.0, 214.925982)
}

exe_path = ".\\build\\src\\Debug\\pathwyse.exe"
set_file = "pathwyse.set"
p_value = 0.25

# --- 2. FUNZIONI DI SUPPORTO ---
def aggiorna_parametri(t, td, k, kd, seed):
    with open(set_file, 'r') as f:
        contenuto = f.read()
    contenuto = re.sub(r'ils_t\s*=\s*[0-9.]+', f'ils_t = {t}', contenuto)
    contenuto = re.sub(r'ils_td\s*=\s*[0-9.]+', f'ils_td = {td}', contenuto)
    contenuto = re.sub(r'ils_k\s*=\s*[0-9]+', f'ils_k = {k}', contenuto)
    contenuto = re.sub(r'ils_kd\s*=\s*[0-9]+', f'ils_kd = {kd}', contenuto)
    contenuto = re.sub(r'ils_p\s*=\s*[0-9.]+', f'ils_p = {p_value}', contenuto)
    contenuto = re.sub(r'seed\s*=\s*[0-9]+', f'seed = {seed}', contenuto)
    with open(set_file, 'w') as f:
        f.write(contenuto)

def esegui_pathwyse(istanza):
    risultato = subprocess.run([exe_path, istanza], capture_output=True, text=True)
    output = risultato.stdout
    match_obj = re.search(r'Obj\s*:\s*(-?\d+\.\d+)', output)
    match_time = re.search(r'Time\s*:\s*(\d+\.\d+)', output)
    if match_obj and match_time:
        return float(match_obj.group(1)), float(match_time.group(1))
    else:
        return None, None

# --- 3. ESECUZIONE TEST E RACCOLTA DATI ---
risultati_dettagliati = []
risultati_aggregati = []

for istanza in istanze:
    print(f"\n--- Testando l'istanza: {istanza} ---")
    pw_val, pw_time = pw_defaults.get(istanza, (0, 0))
    nome_istanza = os.path.basename(istanza)
    
    for combo in combinazioni:
        print(f"  > Eseguendo Combo {combo['id']} (t={combo['t']}, td={combo['td']}, k={combo['k']}, kd={combo['kd']})")
        valori_obj = []
        tempi = []
        
        for seed in seeds:
            aggiorna_parametri(combo['t'], combo['td'], combo['k'], combo['kd'], seed)
            obj, tempo = esegui_pathwyse(istanza)
            
            if obj is not None:
                gap_singolo = ((obj - pw_val) / abs(pw_val)) * 100 if pw_val != 0 else 0
                
                # Salviamo il singolo iterazione
                risultati_dettagliati.append({
                    "Istanza": nome_istanza,
                    "Combinazione": combo["id"],
                    "Seed": seed,
                    "Soluzione_PW": pw_val,
                    "Tempo_PW": pw_time,
                    "Soluzione_Euristica": obj,
                    "Tempo_Euristica": tempo,
                    "Gap_%": round(gap_singolo, 2)
                })
                valori_obj.append(obj)
                tempi.append(tempo)
        
        # Calcolo Medie per il Riepilogo Aggregato
        if valori_obj:
            media_obj = statistics.mean(valori_obj)
            std_obj = statistics.stdev(valori_obj) if len(valori_obj) > 1 else 0
            media_tempo = statistics.mean(tempi)
            std_tempo = statistics.stdev(tempi) if len(tempi) > 1 else 0
            gap_medio = ((media_obj - pw_val) / abs(pw_val)) * 100 if pw_val != 0 else 0
            
            risultati_aggregati.append({
                "Istanza": nome_istanza,
                "Combinazione": combo["id"],
                "Soluzione_PW": pw_val,
                "Tempo_PW": pw_time,
                "Media_Obj": round(media_obj, 2),
                "Std_Obj": round(std_obj, 2),
                "Media_Tempo": round(media_tempo, 4),
                "Std_Tempo": round(std_tempo, 4),
                "Gap_Medio_%": round(gap_medio, 2)
            })

# --- 4. SALVATAGGIO IN DUE CSV ---
with open('risultati_dettagliati.csv', 'w', newline='') as csvfile1:
    campi1 = ["Istanza", "Combinazione", "Seed", "Soluzione_PW", "Tempo_PW", "Soluzione_Euristica", "Tempo_Euristica", "Gap_%"]
    writer1 = csv.DictWriter(csvfile1, fieldnames=campi1)
    writer1.writeheader()
    writer1.writerows(risultati_dettagliati)

with open('risultati_aggregati.csv', 'w', newline='') as csvfile2:
    campi2 = ["Istanza", "Combinazione", "Soluzione_PW", "Tempo_PW", "Media_Obj", "Std_Obj", "Media_Tempo", "Std_Tempo", "Gap_Medio_%"]
    writer2 = csv.DictWriter(csvfile2, fieldnames=campi2)
    writer2.writeheader()
    writer2.writerows(risultati_aggregati)

print("\nFinito! Generati 'risultati_dettagliati.csv' e 'risultati_aggregati.csv'.")