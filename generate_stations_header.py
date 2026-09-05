import json

with open('d:/ESP32Radio/stations_full.json', 'r', encoding='utf-8') as f:
    stations = json.load(f)

clean_stations = []
states_set = set()
languages_set = set()

for s in stations:
    name = s.get('name', '').strip().replace('"', '\\"')
    url = s.get('stream_url', '').strip().replace('"', '\\"')
    state = s.get('state', '').strip().title().replace('"', '\\"')
    lang = s.get('language', '').strip()
    
    if not name or not url:
        continue
        
    primary_lang = lang.split(',')[0].strip()
    if not primary_lang:
        primary_lang = 'General'
        
    clean_stations.append({
        'name': name,
        'state': state if state else 'National',
        'language': primary_lang,
        'url': url
    })
    states_set.add(state if state else 'National')
    languages_set.add(primary_lang)

priority_langs = ['Malayalam', 'Tamil', 'Hindi', 'Telugu', 'Kannada', 'Bengali', 'Marathi', 'Gujarati', 'Punjabi', 'English', 'Urdu', 'Odia', 'Assamese']
ordered_langs = ['All Languages'] + [l for l in priority_langs if l in languages_set] + [l for l in sorted(list(languages_set)) if l not in priority_langs]

priority_states = ['Kerala', 'Tamil Nadu', 'Karnataka', 'Andhra Pradesh', 'Telangana', 'Maharashtra', 'Delhi', 'West Bengal', 'Gujarat', 'Punjab', 'Uttar Pradesh']
ordered_states = ['All States'] + [s for s in priority_states if s in states_set] + [s for s in sorted(list(states_set)) if s not in priority_states]

out_lines = []
out_lines.append('#ifndef _STATIONS_DB_H_')
out_lines.append('#define _STATIONS_DB_H_')
out_lines.append('')
out_lines.append('#include <Arduino.h>')
out_lines.append('')
out_lines.append('struct StationItem {')
out_lines.append('    const char* name;')
out_lines.append('    const char* state;')
out_lines.append('    const char* language;')
out_lines.append('    const char* url;')
out_lines.append('};')
out_lines.append('')
out_lines.append(f'#define TOTAL_ALL_STATIONS {len(clean_stations)}')
out_lines.append(f'#define TOTAL_LANGUAGES {len(ordered_langs)}')
out_lines.append(f'#define TOTAL_STATES {len(ordered_states)}')
out_lines.append('')
out_lines.append('const StationItem ALL_STATIONS[TOTAL_ALL_STATIONS] PROGMEM = {')
for s in clean_stations:
    out_lines.append(f'    {{ "{s["name"]}", "{s["state"]}", "{s["language"]}", "{s["url"]}" }},')
out_lines.append('};')
out_lines.append('')
out_lines.append('const char* const FILTER_LANGUAGES[TOTAL_LANGUAGES] = {')
for l in ordered_langs:
    out_lines.append(f'    "{l}",')
out_lines.append('};')
out_lines.append('')
out_lines.append('const char* const FILTER_STATES[TOTAL_STATES] = {')
for st in ordered_states:
    out_lines.append(f'    "{st}",')
out_lines.append('};')
out_lines.append('')
out_lines.append('#endif // _STATIONS_DB_H_')

with open('d:/ESP32Radio/ESP32S3_JC3248W535_Radio/stations_db.h', 'w', encoding='utf-8') as f:
    f.write('\n'.join(out_lines))

print(f'Successfully generated stations_db.h with {len(clean_stations)} stations!')
