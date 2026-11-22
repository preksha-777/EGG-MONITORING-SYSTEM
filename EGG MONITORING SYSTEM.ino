#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h"

// 1. UPDATE YOUR 2.4 GHz WI-FI DETAILS (Optional - AP backup always works!)
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Backup Direct Hotspot
const char* AP_SSID       = "EggGrader_IoT";
const char* AP_PASS       = "12345678";

// 2. Locked-in Hardware Calibration
const float SENSOR_BASE_HEIGHT_CM = 11.5;
const float CALIBRATION_FACTOR    = 664.0;

// 3. GPIO Pin Definitions
const int TRIG_PIN = 25;
const int ECHO_PIN = 26;
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN  = 33;

HX711 scale;
WebServer server(80);

// 4. Telemetry & Batch State
float currentWeight = 0.0;
float currentHeight = 0.0;
String currentGrade = "EMPTY (Place Egg)";
int countS = 0, countM = 0, countL = 0, countXL = 0, countAnomaly = 0;

// 5-Sample Median Filter for Ultrasonic Height
float measureEggHeight() {
  float readings[5];
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    readings[i] = (duration == 0) ? SENSOR_BASE_HEIGHT_CM : (duration / 58.0);
    delay(15);
  }

  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (readings[j] < readings[i]) {
        float temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }

  float egg_height = SENSOR_BASE_HEIGHT_CM - readings[2];
  return (egg_height < 0.0) ? 0.0 : egg_height;
}

// Multi-Parameter Egg Classifier
String classifyEgg(float w, float h) {
  if (w < 20.0) return "EMPTY (Place Egg)";
  if (w < 45.0) return (h > 5.2) ? "SMALL (Tall Anomaly)" : "SMALL (Grade S)";
  if (w < 53.0) return (h > 5.7) ? "MEDIUM (Tall Anomaly)" : "MEDIUM (Grade M)";
  if (w < 60.0) return (h > 6.2) ? "LARGE (Tall Anomaly)" : "LARGE (Grade L)";
  return "EXTRA LARGE (Grade XL)";
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>Smart Egg Grading IoT Station</title><style>";
  html += ":root{--bg:#0b0f19;--card:#131a2a;--bdr:#1e293b;--acc:#f59e0b;--grn:#10b981;--blu:#3b82f6;--red:#ef4444;--txt:#f8fafc;--mut:#94a3b8;}";
  html += "*{box-sizing:border-box;margin:0;padding:0;font-family:system-ui,sans-serif;}";
  html += "body{background:var(--bg);color:var(--txt);padding:16px;display:flex;justify-content:center;}";
  html += ".box{width:100%;max-width:500px;display:flex;flex-direction:column;gap:14px;}";
  html += ".hdr{display:flex;justify-content:space-between;align-items:center;}";
  html += ".hdr h1{font-size:20px;color:var(--acc);}.hdr p{font-size:12px;color:var(--mut);}";
  html += ".bdg{display:flex;align-items:center;gap:6px;background:rgba(16,185,129,.12);color:var(--grn);padding:6px 12px;border-radius:20px;font-size:12px;font-weight:600;border:1px solid rgba(16,185,129,.3);}";
  html += ".dot{width:8px;height:8px;background:var(--grn);border-radius:50%;}";
  html += ".card{background:var(--card);border:1px solid var(--bdr);border-radius:16px;padding:18px;}";
  html += ".ctitle{font-size:11px;text-transform:uppercase;letter-spacing:1px;color:var(--mut);margin-bottom:12px;font-weight:600;}";
  html += ".stage{display:flex;align-items:center;gap:16px;background:#0d1322;padding:16px;border-radius:12px;border:1px solid var(--bdr);}";
  html += ".egg-box{width:80px;height:92px;display:flex;align-items:center;justify-content:center;}";
  html += ".egg{width:44px;height:58px;background:#475569;border-radius:50% 50% 50% 50%/62% 62% 38% 38%;transition:all .3s;}";
  html += ".gpill{font-size:20px;font-weight:800;color:var(--grn);margin-bottom:4px;}.hint{font-size:12px;color:var(--mut);}";
  html += ".meters{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:14px;}";
  html += ".meter{background:#0d1322;padding:12px;border-radius:12px;border:1px solid var(--bdr);}";
  html += ".mtop{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:8px;}";
  html += ".mval{font-size:22px;font-weight:700;}.mlbl{font-size:12px;color:var(--mut);}";
  html += ".bar{width:100%;height:6px;background:var(--bdr);border-radius:4px;overflow:hidden;}";
  html += ".fill{height:100%;width:0%;background:var(--acc);transition:width .3s;}";
  html += ".brow{display:grid;grid-template-columns:1.4fr 1fr;gap:10px;margin-top:14px;}";
  html += "button{border:none;padding:12px;border-radius:10px;font-size:14px;font-weight:700;cursor:pointer;}";
  html += ".blog{background:var(--acc);color:#0b0f19;}.btare{background:var(--bdr);color:var(--txt);}";
  html += ".bsm{padding:6px 10px;font-size:12px;border-radius:8px;}";
  html += ".bexp{background:rgba(59,130,246,.15);color:var(--blu);border:1px solid rgba(59,130,246,.3);}";
  html += ".brst{background:rgba(239,68,68,.15);color:var(--red);border:1px solid rgba(239,68,68,.3);}";
  html += ".sgrid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;}";
  html += ".sbox{background:#0d1322;padding:10px 6px;border-radius:10px;text-align:center;border:1px solid var(--bdr);}";
  html += ".snum{font-size:20px;font-weight:800;}.stag{font-size:11px;color:var(--mut);}";
  html += ".abar{margin-top:10px;background:rgba(239,68,68,.1);border:1px solid rgba(239,68,68,.3);border-radius:10px;padding:10px 14px;display:flex;justify-content:space-between;align-items:center;color:#fca5a5;font-size:13px;font-weight:600;}";
  html += ".lhdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;}";
  html += ".twrap{max-height:140px;overflow-y:auto;border-radius:8px;border:1px solid var(--bdr);}";
  html += "table{width:100%;border-collapse:collapse;font-size:12px;text-align:left;}";
  html += "th{background:#0d1322;color:var(--mut);padding:8px;}td{padding:8px;border-top:1px solid var(--bdr);}";
  html += "</style></head><body><div class='box'>";
  html += "<div class='hdr'><div><h1>Smart Egg Grader</h1><p>ESP32 Dual-Sensor IoT Station</p></div>";
  html += "<div class='bdg'><span class='dot'></span><span id='st'>LIVE</span></div></div>";
  html += "<div class='card'><div class='ctitle'>Real-Time Classification</div><div class='stage'>";
  html += "<div class='egg-box'><div class='egg' id='ev'></div></div>";
  html += "<div><div class='gpill' id='gr'>EMPTY (Place Egg)</div><div class='hint' id='ht'>Place an egg in the cradle</div></div></div>";
  html += "<div class='meters'><div class='meter'><div class='mtop'><span class='mlbl'>MASS</span><span class='mval' id='wt'>0.0 g</span></div><div class='bar'><div class='fill' id='wb'></div></div></div>";
  html += "<div class='meter'><div class='mtop'><span class='mlbl'>HEIGHT</span><span class='mval' id='hg'>0.0 cm</span></div><div class='bar'><div class='fill' id='hb' style='background:var(--blu)'></div></div></div></div>";
  html += "<div class='brow'><button class='blog' onclick='logEgg()'>+ Log Current Egg</button><button class='btare' onclick=\"fetch('/tare')\">Zero Scale (Tare)</button></div></div>";
  html += "<div class='card'><div class='ctitle'>Batch Summary (Total: <span id='tot'>0</span>)</div><div class='sgrid'>";
  html += "<div class='sbox'><div class='snum' id='cS'>0</div><div class='stag'>Small</div></div>";
  html += "<div class='sbox'><div class='snum' id='cM'>0</div><div class='stag'>Medium</div></div>";
  html += "<div class='sbox'><div class='snum' id='cL'>0</div><div class='stag'>Large</div></div>";
  html += "<div class='sbox'><div class='snum' id='cXL'>0</div><div class='stag'>XL</div></div></div>";
  html += "<div class='abar'><span>Tall Anomalies Flagged</span><span style='font-size:18px;font-weight:800' id='cA'>0</span></div></div>";
  html += "<div class='card'><div class='lhdr'><div class='ctitle' style='margin:0'>Recent Scan History</div>";
  html += "<div><button class='bsm bexp' onclick='expCSV()'>Export CSV</button> <button class='bsm brst' onclick='rstAll()'>Reset</button></div></div>";
  html += "<div class='twrap'><table><thead><tr><th>#</th><th>Time</th><th>Weight</th><th>Height</th><th>Grade</th></tr></thead>";
  html += "<tbody id='tb'><tr><td colspan='5' style='text-align:center;color:#94a3b8;padding:14px'>No eggs logged yet</td></tr></tbody></table></div></div></div>";
  html += "<script>";
  html += "let cur={weight:0,height:0,grade:'EMPTY (Place Egg)'},logs=[];";
  html += "function upd(d){cur=d;document.getElementById('wt').innerText=d.weight.toFixed(1)+' g';";
  html += "document.getElementById('hg').innerText=d.height.toFixed(1)+' cm';";
  html += "document.getElementById('wb').style.width=Math.min(100,(d.weight/75)*100)+'%';";
  html += "document.getElementById('hb').style.width=Math.min(100,(d.height/7.5)*100)+'%';";
  html += "const g=document.getElementById('gr'),e=document.getElementById('ev'),h=document.getElementById('ht');g.innerText=d.grade;";
  html += "if(d.grade.startsWith('EMPTY')){g.style.color='#94a3b8';e.style.background='#334155';e.style.transform='scale(.85)';h.innerText='Ready for next sample';}";
  html += "else if(d.grade.includes('Anomaly')){g.style.color='#ef4444';e.style.background='#ef4444';e.style.transform='scale(.95,1.25)';h.innerText='Height exceeds carton limit!';}";
  html += "else{g.style.color='#10b981';e.style.background='#f59e0b';e.style.transform='scale(1.05)';h.innerText='Normal packaging range';}";
  html += "document.getElementById('cS').innerText=d.cS;document.getElementById('cM').innerText=d.cM;";
  html += "document.getElementById('cL').innerText=d.cL;document.getElementById('cXL').innerText=d.cXL;";
  html += "document.getElementById('cA').innerText=d.cA;document.getElementById('tot').innerText=d.cS+d.cM+d.cL+d.cXL;}";
  html += "function logEgg(){if(cur.weight<20){alert('Place an egg (>20g) first!');return;}";
  html += "fetch('/log').then(()=>{logs.unshift({id:logs.length+1,t:new Date().toLocaleTimeString(),w:cur.weight.toFixed(1),h:cur.height.toFixed(1),g:cur.grade});rend();});}";
  html += "function rend(){const tb=document.getElementById('tb');";
  html += "tb.innerHTML=logs.length?logs.map(r=>`<tr><td>${r.id}</td><td>${r.t}</td><td>${r.w}g</td><td>${r.h}cm</td><td>${r.g}</td></tr>`).join(''):'<tr><td colspan=\"5\" style=\"text-align:center;color:#94a3b8;padding:14px\">No eggs logged yet</td></tr>';}";
  html += "function rstAll(){fetch('/reset').then(()=>{logs=[];rend();});}";
  html += "function expCSV(){if(!logs.length)return;let c='ID,Time,Weight_g,Height_cm,Grade\\n'+logs.map(r=>`${r.id},${r.t},${r.w},${r.h},\"${r.g}\"`).join('\\n');";
  html += "const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([c],{type:'text/csv'}));a.download='egg_batch.csv';a.click();}";
  html += "setInterval(()=>fetch('/data').then(r=>r.json()).then(d=>{document.getElementById('st').innerText='LIVE';upd(d);}).catch(()=>document.getElementById('st').innerText='OFFLINE'),500);";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"weight\":" + String(currentWeight, 1) + ",";
  json += "\"height\":" + String(currentHeight, 1) + ",";
  json += "\"grade\":\"" + currentGrade + "\",";
  json += "\"cS\":" + String(countS) + ",";
  json += "\"cM\":" + String(countM) + ",";
  json += "\"cL\":" + String(countL) + ",";
  json += "\"cXL\":" + String(countXL) + ",";
  json += "\"cA\":" + String(countAnomaly);
  json += "}";
  server.send(200, "application/json", json);
}

void handleLog() {
  if (currentWeight >= 20.0) {
    if (currentGrade.indexOf("Anomaly") >= 0) countAnomaly++;
    if (currentGrade.startsWith("SMALL")) countS++;
    else if (currentGrade.startsWith("MEDIUM")) countM++;
    else if (currentGrade.startsWith("LARGE")) countL++;
    else if (currentGrade.startsWith("EXTRA")) countXL++;
  }
  server.send(200, "text/plain", "OK");
}

void handleTare() {
  scale.tare();
  server.send(200, "text/plain", "TARED");
}

void handleReset() {
  countS = countM = countL = countXL = countAnomaly = 0;
  server.send(200, "text/plain", "RESET");
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (scale.wait_ready_timeout(2000)) {
    scale.set_scale(CALIBRATION_FACTOR);
    scale.tare();
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Direct Hotspot IP: http://");
  Serial.println(WiFi.softAPIP());

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to router Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nRouter Wi-Fi IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nRouter skipped - connect to Wi-Fi 'EggGrader_IoT' (pass: 12345678) and open http://192.168.4.1");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/log", handleLog);
  server.on("/tare", handleTare);
  server.on("/reset", handleReset);
  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 500) {
    lastRead = millis();
    if (scale.is_ready()) {
      currentWeight = scale.get_units(5);
      if (currentWeight < 0.5) currentWeight = 0.0;
    }
    currentHeight = measureEggHeight();
    currentGrade = classifyEgg(currentWeight, currentHeight);
  }
}