#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "Settings.h"

String getPage() {
    String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    html += F("<title>ESP32 Control Pro</title>");
    html += F("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>");
    html += F("<style>");
    html += F("body{font-family:sans-serif;background:#121212;color:#e0e0e0;text-align:center;margin:0;}");
    html += F("nav{background:#1f1f1f;padding:12px;display:flex;justify-content:center;gap:10px;border-bottom:1px solid #333;}");
    html += F("nav button{padding:10px 15px;background:#333;color:#aaa;border:none;border-radius:6px;cursor:pointer;}");
    html += F("nav button.active{background:#00e676;color:#000;font-weight:bold;}");
    html += F(".page{display:none;padding:15px;animation:fadeIn 0.3s;} .page.active{display:block;}");
    html += F(".card{background:#1e1e1e;padding:15px;margin:10px auto;max-width:600px;border-radius:12px;box-shadow:0 4px 10px rgba(0,0,0,0.5);}");
    html += F(".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:15px;}");
    html += F(".val-box{background:#262626;padding:10px;border-radius:10px;border:1px solid #333;}");
    html += F(".label{font-size:0.8em;color:#888;} .value{font-size:1.5em;font-weight:bold;color:#00e676;}");
    html += F("input[type='number']{background:#333;color:#fff;border:1px solid #444;padding:8px;border-radius:5px;width:70px;font-size:1.2em;text-align:center;}");
    html += F("button.set-btn{background:#00e676;color:#000;border:none;padding:8px 15px;border-radius:5px;font-weight:bold;cursor:pointer;margin-left:5px;}");
    if (connectStat) html += F("button.set-wifi{background:#00e676;color:#000;border:none;padding:8px 15px;border-radius:5px;font-weight:bold;cursor:pointer;margin-left:5px;}");
    html += F("table{width:100%;margin-top:10px;border-collapse:collapse;} td,th{padding:12px;border-bottom:1px solid #333;text-align:left;}");
    html += F(".relay-status{display:inline-block;padding:3px 10px;border-radius:15px;font-size:0.8em;font-weight:bold;}");
    html += F(".on{background:#1b5e20;color:#00e676;} .off{background:#b71c1c;color:#ff8a80;}");

    // html += F(".ota-bar{display:none;background:#ff9800;color:#000;padding:15px;position:fixed;bottom:0;left:0;right:0;text-align:center;font-weight:bold;z-index:1000;box-shadow:0 -2px 10px rgba(0,0,0,0.5);}");
    // html += F(".ota-btn{background:#000;color:#ff9800;border:none;padding:8px 15px;border-radius:5px;margin-left:10px;cursor:pointer;font-weight:bold;}");
    
    html += F("@keyframes fadeIn{from{opacity:0;} to{opacity:1;}}");
    html += F("</style></head><body>");

    html += F("<nav><button onclick='showPage(\"p1\", this)' class='active'>Главная</button>");
    html += F("<button onclick='showPage(\"p2\", this)'>Архив</button>");
    html += F("<button onclick='showPage(\"p3\", this)'>Датчики</button></nav>");

    // СТРАНИЦА 1: ГЛАВНАЯ
    html += F("<div id='p1' class='page active'>");
    html += F("<div class='card'><h3>Управление и мониторинг</h3>");
    html += F("<div class='grid'>");
    html += F("<div class='val-box'><div class='label'>Внутри (средняя)</div><div class='value' id='v_avg'>--</div></div>");
    html += F("<div class='val-box'><div class='label'>Снаружи</div><div class='value' style='color:#ffcc00' id='v_out'>--</div></div>");
    html += F("</div>");
    
    // Блок установки температуры
    html += F("<div class='val-box' style='margin-bottom:15px;'>");
    html += F("<div class='label'>Целевая температура (°C)</div>");
    html += F("<div style='margin-top:5px;'><input type='number' id='targetInp' step='0.5' value=''>");
    html += F("<button class='set-btn' onclick='setTemp()'>ОК</button></div>");
    html += F("<div class='label' style='margin-top:5px;'>Сейчас установлено: <b id='v_target' style='color:#fff'>--</b>°C</div></div>");

    html += F("<div class='grid'>");
    html += F("<div class='val-box'><div class='label'>Нижний нагрев</div><div id='r1_stat' class='relay-status off'>--</div></div>");
    html += F("<div class='val-box'><div class='label'>Верхний нагрев</div><div id='r2_stat' class='relay-status off'>--</div></div>");
    html += F("</div>");
    html += F("<canvas id='rtChart'></canvas></div>");
    // Вставьте это в getPage() вместо старого блока обновлений
    html += F("<div class='val-box' style='margin-top:20px;'>");
    html += F("<a href='/ota_page' style='color:#2196F3; text-decoration:none; font-weight:bold;'>");
    html += F("⚙ Обновление ПО</a>");
    if (connectStat) {
    html += F("</div><div class='val-box' style='margin-top:20px;'>");
      html += F("<a href='/config' style='color:#2196F3; text-decoration:none; font-weight:bold;'>");
      html += F("⚙ Настройка WiFi</a>");
    }
    html += F("</div></div>");

    // СТРАНИЦА 2: АРХИВ
    html += F("<div id='p2' class='page'><div class='card'><h2>История за сутки</h2>");
    html += F("<input type='date' id='hDate' onchange='loadHist()' style='padding:8px; background:#333; color:#fff; border:1px solid #444; border-radius:5px;'>");
    html += F("<canvas id='hChart' style='margin-top:15px;'></canvas></div></div>");

    // СТРАНИЦА 3: ВСЕ ДАТЧИКИ
    html += F("<div id='p3' class='page'><div class='card'><h2>Показания всех датчиков</h2>");
    html += F("<table><tr><th>Датчик</th><th>Температура</th></tr>");
    html += F("<tr><td>Внизу</td><td class='value' id='d0'>--</td></tr>");
    html += F("<tr><td>У двери</td><td class='value' id='d1'>--</td></tr>");
    html += F("<tr><td>Середина</td><td class='value' id='d2'>--</td></tr>");
    html += F("<tr><td>Верху</td><td class='value' id='d3'>--</td></tr>");
    html += F("<tr><td>Снаружи</td><td class='value' style='color:#ffcc00' id='d4'>--</td></tr>");
    html += F("</table></div></div>");

    html += F("<script>");
    html += F("function showPage(id, btn){document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));");
    html += F("document.querySelectorAll('nav button').forEach(b=>b.classList.remove('active'));");
    html += F("document.getElementById(id).classList.add('active'); btn.classList.add('active');}");

    // График RT
    html += F("var rtCtx = document.getElementById('rtChart').getContext('2d');");
    html += F("var rtChart = new Chart(rtCtx, {type:'line', data:{labels:Array(30).fill(''),");
    html += F("datasets:[{label:'Внутри', borderColor:'#00e676', data:[], tension:0.3, pointRadius:0},");
    html += F("{label:'Снаружи', borderColor:'#ffcc00', data:[], tension:0.3, pointRadius:0}]},");
    html += F("options:{animation:false, scales:{y:{grid:{color:'#333'}},x:{display:false}}}});");

    // Функция обновления данных
    html += F("function update(){ fetch('/get_data').then(r=>r.json()).then(d=>{");
    html += F("document.getElementById('v_avg').innerText=d.avg+'°';");
    html += F("document.getElementById('v_out').innerText=d.out+'°';");
    html += F("document.getElementById('v_target').innerText=d.target;");
    html += F("document.getElementById('d0').innerText=d.d0+'°C'; document.getElementById('d1').innerText=d.d1+'°C';");
    html += F("document.getElementById('d2').innerText=d.d2+'°C'; document.getElementById('d3').innerText=d.d3+'°C';");
    html += F("document.getElementById('d4').innerText=d.d4+'°C';");
    html += F("let r1=document.getElementById('r1_stat'); r1.innerText=d.s1?'НАГРЕВ':'ВЫКЛ'; r1.className='relay-status '+(d.s1?'on':'off');");
    html += F("let r2=document.getElementById('r2_stat'); r2.innerText=d.s2?'НАГРЕВ':'ВЫКЛ'; r2.className='relay-status '+(d.s2?'on':'off');");
    html += F("rtChart.data.datasets[0].data=d.histIn; rtChart.data.datasets[1].data=d.histOut; rtChart.update();");
    html += F("}); } setInterval(update, 3000); update();");

    // Новая функция для запуска обновления
    html += F("function doActualUpdate() {");
    html += F("if(confirm('Устройство будет перезагружено для установки обновления. Продолжить?')) {");
    html += F("document.getElementById('ota_bar').innerText = 'Загрузка прошивки... Не выключайте питание!';");
    html += F("fetch('/do_ota');");
    html += F("}");
    html += F("}");
    // Отправка новой температуры
    html += F("function setTemp(){ let t=document.getElementById('targetInp').value; fetch('/set?temp='+t, {method:'POST'}).then(()=>update()); }");

    // Архив
    html += F("var hChart = new Chart(document.getElementById('hChart'), {");
    html += F(" type:'line', ");
    html += F(" data:{labels:[], ");
    html += F(" datasets:[");
    html += F("   {");
    html += F("     label:'Внутри',");
    html += F("     borderColor:'#00e676',");
    html += F("     data:[]},");
    html += F("   {");
    html += F("     label:'Снаружи',");
    html += F("     borderColor:'#ffcc00', ");
    html += F("     data:[]}]}});");
    html += F("function loadHist(){ ");
    html += F(" let d=document.getElementById('hDate').value;");
    html += F(" fetch('/get_history?date='+d).then(r=>r.json()).then(data=>{");
    html += F("hChart.data.labels=data.map(i=>i.h+':00'); hChart.data.datasets[0].data=data.map(i=>i.t1); hChart.data.datasets[1].data=data.map(i=>i.t2); hChart.update(); }); }");
    html += F("</script></body></html>");
    return html;
}

String getOTAPage(bool found, String v, String n) {
    String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    html += F("<style>body{font-family:sans-serif;background:#121212;color:#eee;text-align:center;padding:20px;}");
    html += F(".card{background:#1e1e1e;padding:20px;border-radius:10px;max-width:400px;margin:auto;border:1px solid #333;}");
    html += F("button{padding:12px 20px;margin:10px;border:none;border-radius:5px;cursor:pointer;font-weight:bold;}");
    html += F(".btn-check{background:#2196F3;color:#fff;} .btn-upd{background:#ff9800;color:#000;}");
    html += F(".btn-back{background:#555;color:#eee;text-decoration:none;display:inline-block;padding:10px;}");
    html += F("</style></head><body>");

    html += F("<div class='card'><h2>Обновление ПО</h2>");
    html += F("<p>Текущая версия: ");
    html += F(VERSION);
    html += F("</p>");

    if (found) {
        html += "<p style='color:#ff9800'>Найдена новая версия: <b>" + v + "</b></p>";
        if (n.length() > 0) html += "<p style='font-size:0.9em;color:#888;'>" + n + "</p>";
        html += F("<form action='/do_ota' method='GET'><button class='btn-upd'>УСТАНОВИТЬ ОБНОВЛЕНИЕ</button></form>");
    } else {
        html += F("<p>Обновлений не найдено или проверка не проводилась.</p>");
        html += F("<form action='/check_manual' method='GET'><button class='btn-check'>ПРОВЕРИТЬ НАЛИЧИЕ</button></form>");
    }

    html += F("<br><br><a href='/' class='btn-back'>назад на главную</a>");
    html += F("</div></body></html>");
    return html;
}

/*void handleData() {
    String json = "{";
    json += "\"avg\":" + String(tempIn[4], 1) + ",";
    json += "\"out\":" + String(tempOut, 1) + ",";
    json += "\"target\":" + String(targetTemp, 1) + ",";
    // Добавляем статус обновления
    json += "\"otaAvail\":" + String(updateAvailable ? "true" : "false") + ",";
    json += "\"newVer\":\"" + ver + "\","; // Версия из GitHub
    json += "\"s1\":" + String(relay1Stat ? "true" : "false") + ",";
    json += "\"s2\":" + String(relay2Stat ? "true" : "false") + ",";
    json += "\"d0\":" + String(tempIn[0], 1) + ",";
    json += "\"d1\":" + String(tempIn[1], 1) + ",";
    json += "\"d2\":" + String(tempIn[2], 1) + ",";
    json += "\"d3\":" + String(tempIn[3], 1) + ",";
    json += "\"d4\":" + String(tempOut, 1) + ",";
    json += "\"histIn\": [";
    for (int i = 0; i < 30; i++) { json += String(historyRT[i], 1); if (i < 29) json += ","; }
    json += "], \"histOut\": [";
    for (int i = 0; i < 30; i++) { json += String(historyRT_Out[i], 1); if (i < 29) json += ","; }
    json += "]}";
    server.send(200, "application/json", json);
}*/

void handleData() {
    String json = "{";
    json.reserve(1024);
    // Вспомогательная лямбда-функция для проверки float на nan
    auto checkNan = [](float val) -> String {
        return isnan(val) ? "null" : String(val, 1);
    };

    json += "\"avg\":" + checkNan(tempIn[4]) + ",";
    json += "\"out\":" + checkNan(tempOut) + ",";
    json += "\"target\":" + String(targetTemp, 1) + ",";
    
    // Статус обновления
    json += "\"otaAvail\":" + String(updateAvailable ? "true" : "false") + ",";
    json += "\"newVer\":\"" + ver + "\","; 
    
    json += "\"s1\":" + String(relay1Stat ? "true" : "false") + ",";
    json += "\"s2\":" + String(relay2Stat ? "true" : "false") + ",";
    
    // Датчики по отдельности
    json += "\"d0\":" + checkNan(tempIn[0]) + ",";
    json += "\"d1\":" + checkNan(tempIn[1]) + ",";
    json += "\"d2\":" + checkNan(tempIn[2]) + ",";
    json += "\"d3\":" + checkNan(tempIn[3]) + ",";
    json += "\"d4\":" + checkNan(tempOut) + ",";
    
    // Массив истории Внутри
    json += "\"histIn\": [";
    for (int i = 0; i < 30; i++) { 
        json += checkNan(historyRT[i]); 
        if (i < 29) json += ","; 
    }
    
    // Массив истории Улица
    json += "], \"histOut\": [";
    for (int i = 0; i < 30; i++) { 
        json += checkNan(historyRT_Out[i]); 
        if (i < 29) json += ","; 
    }
    
    json += "]}";
    server.send(200, "application/json", json);
}

void handleGetHistory() {
    String date = server.arg("date");
    String filename = "/" + date + ".txt";
    if (!LittleFS.exists(filename)) { 
        server.send(200, "application/json", "[]"); 
        return; 
    }
    File file = LittleFS.open(filename, FILE_READ);
    Serial.print("File load: ");
    Serial.println(filename);

    String json = "[";
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim(); // Убираем лишние пробелы и символы переноса
        if (line.length() == 0) continue;
        // Формат файла: час,темпВнутр,темпУлица
        int c1 = line.indexOf(',');
        int c2 = line.lastIndexOf(',');

        if (c1 > 0 && c2 > c1) {
            String sH = line.substring(0, c1);
            String sT1 = line.substring(c1 + 1, c2);
            String sT2 = line.substring(c2 + 1);

            // Превращаем в float для проверки на nan
            float fT1 = sT1.toFloat();
            float fT2 = sT2.toFloat();

            json += "{\"h\":" + sH;
            json += ",\"t1\":" + String(isnan(fT1) ? "null" : String(fT1, 1));
            json += ",\"t2\":" + String(isnan(fT2) ? "null" : String(fT2, 1)) + "}";
            if (file.available()) json += ",";
        }
    }
    // Убираем возможную лишнюю запятую в конце (если последняя строка была битой)
    if (json.endsWith(",")) json.remove(json.length() - 1);

    json += "]";
    Serial.println(json);
    file.close();
    server.send(200, "application/json", json);
}

#endif
