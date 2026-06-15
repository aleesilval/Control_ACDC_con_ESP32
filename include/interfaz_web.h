#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Sistema de Control de Motores AC/DC</title>
  <style>
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
    body { background-color: #0f172a; color: #ffffff; font-family: system-ui, sans-serif; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; min-height: 100vh; }
    
    .nav-bar { display: flex; gap: 10px; background-color: #1e293b; padding: 8px; border-radius: 12px; margin-bottom: 20px; width: 100%; max-width: 500px; border: 1px solid #334155; }
    .nav-btn { flex: 1; padding: 12px; background: transparent; border: 1px solid transparent; color: #64748b; font-weight: 800; border-radius: 8px; cursor: pointer; transition: all 0.2s; font-size: 14px; text-transform: uppercase; }
    .nav-btn.active { background-color: #334155; color: #38bdf8; border-color: #475569; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.2); }
    
    .container { background-color: #1e293b; width: 100%; max-width: 500px; padding: 24px; border-radius: 16px; border: 1px solid #334155; display: flex; flex-direction: column; gap: 20px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); position: relative; }
    .tab-content { display: none; flex-direction: column; gap: 20px; }
    .tab-content.active { display: flex; }

    .header { display: flex; justify-content: space-between; align-items: center; width: 100%; border-bottom: 1px solid #334155; padding-bottom: 15px; }
    h1 { margin: 0; font-size: 22px; color: #38bdf8; font-weight: 800; }
    
    /* Panel Latente ZCD */
    .zcd-panel { background: #020617; border: 1px solid #1e293b; padding: 10px 15px; border-radius: 8px; display: flex; justify-content: center; align-items: center; gap: 12px; width: 100%; box-shadow: inset 0 2px 4px rgba(0,0,0,0.5); }
    .zcd-dot { width: 10px; height: 10px; border-radius: 50%; background: #ef4444; box-shadow: 0 0 8px #ef4444; transition: all 0.3s; }
    .zcd-dot.sync { background: #10b981; box-shadow: 0 0 8px #10b981; }
    .zcd-text { font-size: 11px; color: #94a3b8; font-weight: 800; text-transform: uppercase; letter-spacing: 0.5px; }
    .zcd-val { color: #e2e8f0; }

    .btn-maestro { width: 100%; padding: 16px; border-radius: 12px; border: none; font-size: 18px; font-weight: 900; cursor: pointer; text-transform: uppercase; transition: all 0.2s; letter-spacing: 1px; }
    .power-off { background-color: #ef4444; color: white; box-shadow: 0 5px #b91c1c; }
    .power-off:active { transform: translateY(4px); box-shadow: 0 1px #b91c1c; }
    .power-on { background-color: #10b981; color: white; box-shadow: 0 5px #047857; }
    .power-on:active { transform: translateY(4px); box-shadow: 0 1px #047857; }

    .display-box { background-color: #0f172a; padding: 20px; border-radius: 12px; text-align: center; border: 1px solid #334155; position: relative; }
    .lbl { color: #64748b; font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; display: block; margin-bottom: 8px; }
    .value-input { background: transparent; border: none; border-radius: 8px; font-size: 48px; font-weight: 900; color: #facc15; padding: 5px; width: 130px; text-align: center; font-family: inherit; outline: none; }
    
    /* Efectos visuales de rampa y apagado */
    .value-off { color: #475569 !important; }
    .ramping { color: #f97316 !important; text-shadow: 0 0 10px rgba(249, 115, 22, 0.5); }

    /* Estilos de la Onda AC */
    .canvas-container { width: 100%; height: 80px; background: #020617; border-radius: 8px; border: 1px solid #334155; position: relative; overflow: hidden; margin-top: 15px; }
    canvas { display: block; width: 100%; height: 100%; }

    .slider { width: 100%; height: 8px; border-radius: 4px; background: #334155; outline: none; -webkit-appearance: none; margin: 15px 0; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; width: 26px; height: 26px; border-radius: 50%; background: #38bdf8; cursor: pointer; }
    
    .btn-config-trigger { background: #334155; border: 1px solid #475569; color: #94a3b8; font-size: 20px; padding: 8px 12px; border-radius: 8px; cursor: pointer; transition: all 0.2s; }
    .btn-config-trigger:hover { color: #38bdf8; background: #475569; }

    .modal-overlay { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(15, 23, 42, 0.85); backdrop-filter: blur(5px); justify-content: center; align-items: center; z-index: 100; }
    .modal-card { background: #1e293b; width: 90%; max-width: 450px; border-radius: 16px; border: 1px solid #475569; padding: 24px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.5); max-height: 85vh; overflow-y: auto; }
    .modal-header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 12px; margin-bottom: 15px; }
    .modal-title { font-size: 18px; font-weight: 800; color: #38bdf8; margin: 0; }
    .close-btn { background: none; border: none; color: #ef4444; font-size: 24px; cursor: pointer; font-weight: bold; }

    .calib-table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    .calib-table th { color: #64748b; font-size: 11px; text-transform: uppercase; padding: 6px; text-align: center; }
    .calib-row { border-bottom: 1px solid #334155; }
    .calib-row td { padding: 6px; text-align: center; color: #94a3b8; font-size: 14px; }
    .calib-row input { width: 80px; background: #0f172a; border: 1px solid #334155; color: #facc15; text-align: center; border-radius: 6px; padding: 4px; font-weight: bold; }
    
    .config-field { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; background: #0f172a; padding: 10px; border-radius: 8px; border: 1px solid #334155; }
    .config-field label { font-size: 13px; color: #94a3b8; font-weight: bold; }
    .config-field input { width: 80px; background: #1e293b; border: 1px solid #475569; color: #facc15; text-align: center; border-radius: 6px; padding: 6px; font-weight: bold; }
  </style>
  <script>
    let sysPower = false;
    let currentMode = 0; 
    let limiteSupHW = 150; // Para el renderizado del apagado de la onda

    function switchTab(mode) {
      if(sysPower) {
        alert("¡OPERACIÓN DENEGADA!\nEl puente SCR debe estar completamente APAGADO para cambiar de modo.");
        return;
      }
      currentMode = mode;
      document.querySelectorAll('.nav-btn').forEach((btn, idx) => btn.classList.toggle('active', idx === mode));
      document.querySelectorAll('.tab-content').forEach((tab, idx) => tab.classList.toggle('active', idx === mode));
      fetch('/modo?val=' + mode);
    }

    function togglePower() {
      sysPower = !sysPower;
      const btn = document.getElementById('powerBtn');
      btn.innerText = sysPower ? '🔴 DETENER SISTEMA' : '🟢 INICIAR SISTEMA';
      btn.className = sysPower ? 'btn-maestro power-off' : 'btn-maestro power-on';
      fetch('/power?state=' + (sysPower ? '1' : '0'));
    }

    function sendAlphaUpdate(val) {
      document.getElementById('sliderAlfa').value = val;
      // No actualizamos el input gigante instantáneamente, dejamos que el servidor lo dibuje suavemente
      fetch('/set?alpha=' + val);
    }

    function sendSetpointUpdate(val) {
      document.getElementById('lblSetpoint').innerText = val + ' RPM';
      fetch('/setpoint?rpm=' + val);
    }

    function toggleModal(show) {
      document.getElementById('panel-hidden-p6').style.display = (currentMode === 1) ? 'block' : 'none';
      document.getElementById('panel-hidden-p7').style.display = (currentMode === 2) ? 'block' : 'none';
      document.getElementById('panel-hidden-p5').style.display = (currentMode === 0) ? 'block' : 'none';
      document.getElementById('configModal').style.display = show ? 'flex' : 'none';
    }

    // Dibujo Dinámico de la Onda
    function drawWave(alphaActual) {
      const canvas = document.getElementById('waveCanvas');
      if(!canvas) return;
      const ctx = canvas.getContext('2d');
      canvas.width = canvas.offsetWidth;
      canvas.height = canvas.offsetHeight;
      const w = canvas.width; const h = canvas.height;
      
      ctx.clearRect(0, 0, w, h);
      ctx.beginPath(); ctx.moveTo(0, h/2); ctx.lineTo(w, h/2);
      ctx.strokeStyle = '#1e293b'; ctx.lineWidth = 1; ctx.stroke();

      for(let x = 0; x < w; x++) {
        let deg = (x / w) * 360; 
        let y = (h/2) - (Math.sin(deg * (Math.PI / 180)) * (h/2 - 10)); 
        let cycleDeg = deg % 180; 
        
        let renderAngle = alphaActual >= limiteSupHW ? 180 : alphaActual;
        ctx.beginPath();
        ctx.arc(x, y, 1.5, 0, Math.PI * 2);
        
        if (!sysPower && alphaActual >= limiteSupHW) {
          ctx.fillStyle = '#475569'; 
        } else if (cycleDeg < renderAngle) {
          ctx.fillStyle = '#475569'; // Parte apagada (Fase recortada)
        } else {
          ctx.fillStyle = '#facc15'; // Parte conduciendo (Energía al motor)
          ctx.lineTo(x, h/2);
          ctx.strokeStyle = 'rgba(250, 204, 21, 0.1)';
          ctx.stroke();
        }
        ctx.fill();
      }
    }

    function saveCalibTable() {
      let params = [];
      for(let i=0; i<10; i++) {
        let v = document.getElementById('v_' + i).value;
        let rpm = document.getElementById('rpm_' + i).value;
        params.push('v' + i + '=' + v + '&rpm' + i + '=' + rpm);
      }
      fetch('/save_calib?' + params.join('&')).then(() => alert("Tabla guardada en memoria Flash."));
    }

    function sendTuneUpdate() {
      const kp = document.getElementById('inputKp').value;
      const ki = document.getElementById('inputKi').value;
      fetch('/tune?kp=' + kp + '&ki=' + ki);
    }

    function sendP5Config() {
      const limInf = document.getElementById('cfgLimInf').value;
      const limSup = document.getElementById('cfgLimSup').value;
      const rampaOn = document.getElementById('cfgRampaOn').value;
      const rampaOff = document.getElementById('cfgRampaOff').value;
      const pulso = document.getElementById('cfgPulso').value;
      
      limiteSupHW = parseInt(limSup);
      document.getElementById('sliderAlfa').min = limInf;
      document.getElementById('sliderAlfa').max = limSup;
      
      fetch('/config_p5?lim_inf=' + limInf + '&lim_sup=' + limSup + '&rampa_on=' + rampaOn + '&rampa_off=' + rampaOff + '&pulso=' + pulso);
    }

    window.onload = function() {
      fetch('/get_config')
        .then(res => res.text())
        .then(data => {
          let vals = data.split(',');
          document.getElementById('inputKp').value = vals[0];
          document.getElementById('inputKi').value = vals[1];
          document.getElementById('sliderSetpoint').value = vals[2];
          document.getElementById('lblSetpoint').innerText = vals[2] + ' RPM';
          currentMode = parseInt(vals[3]);
          sysPower = (vals[4] === '1');
          
          document.getElementById('cfgLimInf').value = vals[5];
          document.getElementById('cfgLimSup').value = vals[6];
          limiteSupHW = parseInt(vals[6]);

          document.getElementById('cfgRampaOn').value = vals[7];
          document.getElementById('cfgRampaOff').value = vals[8];
          document.getElementById('cfgPulso').value = vals[9];

          document.getElementById('sliderAlfa').min = vals[5];
          document.getElementById('sliderAlfa').max = vals[6];
          
          document.querySelectorAll('.nav-btn').forEach((btn, idx) => btn.classList.toggle('active', idx === currentMode));
          document.querySelectorAll('.tab-content').forEach((tab, idx) => tab.classList.toggle('active', idx === currentMode));
          
          const btn = document.getElementById('powerBtn');
          btn.innerText = sysPower ? '🔴 DETENER SISTEMA' : '🟢 INICIAR SISTEMA';
          btn.className = sysPower ? 'btn-maestro power-off' : 'btn-maestro power-on';

          for(let i=0; i<10; i++) {
            document.getElementById('v_' + i).value = vals[10 + i*2];
            document.getElementById('rpm_' + i).value = vals[11 + i*2];
          }
        });
    };

    setInterval(() => {
      fetch('/get')
        .then(res => res.text())
        .then(data => {
          let vals = data.split(',');
          // PAYLOAD: [0]obj, [1]power, [2]actual, [3]rpm, [4]volt, [5]zcd_sync, [6]semiciclo
          let alfaObjetivo = parseInt(vals[0]);
          let sPower = (vals[1] === '1');
          let alfaActual = parseInt(vals[2]);

          if(sysPower !== sPower) { sysPower = sPower; togglePower(); }
          
          // Actualizamos el número visual
          document.getElementById('alphaInput').value = alfaActual;
          
          // Lógica visual de Rampa y Apagado
          if ((sysPower && alfaObjetivo !== alfaActual) || (!sysPower && alfaActual < limiteSupHW)) {
            document.getElementById('alphaInput').className = 'value-input ramping';
          } else if (!sysPower && alfaActual >= limiteSupHW) {
            document.getElementById('alphaInput').className = 'value-input value-off';
          } else {
            document.getElementById('alphaInput').className = 'value-input';
          }

          // Renderizar gráfica
          if(currentMode === 0) drawWave(alfaActual);
          
          document.getElementById('rpmDisplay').innerText = vals[3] + " RPM";
          document.getElementById('voltDisplay').innerText = vals[4] + " V";
          document.getElementById('rpmControlDisplay').innerText = vals[3] + " RPM";

          let zcdSync = (vals[5] === '1');
          let semiciclo = parseInt(vals[6]);
          
          document.getElementById('zcdDot').className = zcdSync ? 'zcd-dot sync' : 'zcd-dot';
          document.getElementById('zcdState').innerText = zcdSync ? 'SINCRONIZADO' : 'BUSCANDO...';
          document.getElementById('zcdTime').innerText = semiciclo;
          
          if(zcdSync && semiciclo > 0) {
             let freq = 1000000.0 / (semiciclo * 2);
             document.getElementById('zcdFreq').innerText = freq.toFixed(1);
          } else {
             document.getElementById('zcdFreq').innerText = "--";
          }
        });
    }, 150);
  </script>
</head>
<body>

  <div class="nav-bar">
    <button class="nav-btn active" onclick="switchTab(0)">P5 (Manual)</button>
    <button class="nav-btn" onclick="switchTab(1)">P6 (Taco)</button>
    <button class="nav-btn" onclick="switchTab(2)">P7 (Lazo C.)</button>
  </div>

  <div class="container">
    <div class="header">
      <h1 id="titleDisplay">Panel Metrológico</h1>
      <button class="btn-config-trigger" onclick="toggleModal(true)">⚙️ Parámetros</button>
    </div>

    <div class="zcd-panel">
      <div class="zcd-dot" id="zcdDot"></div>
      <span class="zcd-text">ZCD: <span id="zcdState" class="zcd-val">Cargando...</span></span>
      <span class="zcd-text" style="color:#334155;">|</span>
      <span class="zcd-text">Semiciclo: <span id="zcdTime" class="zcd-val">--</span> µs</span>
      <span class="zcd-text" style="color:#334155;">|</span>
      <span class="zcd-text">Frec: <span id="zcdFreq" class="zcd-val">--</span> Hz</span>
    </div>

    <button id="powerBtn" onclick="togglePower()" class="btn-maestro power-on">🟢 INICIAR SISTEMA</button>

    <div class="tab-content active" id="tab-manual">
      <div class="display-box">
        <span class="lbl">Ángulo de Disparo Real (α)</span>
        <input type="text" id="alphaInput" class="value-input value-off" value="150" readonly>
        <input type="range" id="sliderAlfa" class="slider" min="10" max="150" value="150" onchange="sendAlphaUpdate(this.value)">
        
        <div class="canvas-container">
          <canvas id="waveCanvas"></canvas>
        </div>
      </div>
    </div>

    <div class="tab-content" id="tab-calib">
      <div class="display-box" style="display: flex; justify-content: space-around;">
        <div><span class="lbl">Velocidad Filtrada</span><div id="rpmDisplay" style="font-size:26px; color:#10b981; font-weight:bold;">0 RPM</div></div>
        <div><span class="lbl">Voltaje en Pin 35</span><div id="voltDisplay" style="font-size:26px; color:#facc15; font-weight:bold;">0.00 V</div></div>
      </div>
    </div>

    <div class="tab-content" id="tab-control">
      <div class="display-box">
        <span class="lbl">Realimentación Dinámica</span>
        <div id="rpmControlDisplay" style="font-size:48px; color:#38bdf8; font-weight:bold;">0 RPM</div>
        
        <span class="lbl" style="margin-top:20px;">Consigna de Velocidad: <span id="lblSetpoint" style="color:#facc15;">0 RPM</span></span>
        <input type="range" id="sliderSetpoint" class="slider" min="0" max="1500" value="0" oninput="document.getElementById('lblSetpoint').innerText = this.value + ' RPM'" onchange="sendSetpointUpdate(this.value)">
      </div>
    </div>
  </div>

  <div id="configModal" class="modal-overlay">
    <div class="modal-card">
      <div class="modal-header">
        <h2 class="modal-title">Configuración Interna</h2>
        <button class="close-btn" onclick="toggleModal(false)">✕</button>
      </div>

      <div id="panel-hidden-p5">
        <span class="lbl">Parámetros Físicos del Puente SCR</span>
        <div class="config-field">
          <label>Umbral de Encendido (Lím. Inf °)</label>
          <input type="number" id="cfgLimInf" value="10">
        </div>
        <div class="config-field">
          <label>Umbral de Apagado (Lím. Sup °)</label>
          <input type="number" id="cfgLimSup" value="150">
        </div>
        <div class="config-field">
          <label>Rampa de Encendido (ms)</label>
          <input type="number" id="cfgRampaOn" value="15">
        </div>
        <div class="config-field">
          <label>Rampa de Apagado (ms)</label>
          <input type="number" id="cfgRampaOff" value="45">
        </div>
        <div class="config-field">
          <label>Ancho Pulso Gate (µs)</label>
          <input type="number" id="cfgPulso" value="1000">
        </div>
        <button onclick="sendP5Config(); toggleModal(false);" style="width:100%; margin-top:10px; padding:12px; background:#f59e0b; border:none; border-radius:8px; color:white; font-weight:bold; cursor:pointer;">APLICAR Y GUARDAR</button>
      </div>

      <div id="panel-hidden-p6">
        <span class="lbl">Curva Estática del Tacogenerador</span>
        <table class="calib-table">
          <thead><tr><th>Punto</th><th>Voltaje (V)</th><th>RPM</th></tr></thead>
          <tbody>
            <script>
              for(let i=0; i<10; i++) {
                document.write('<tr class="calib-row"><td>' + (i+1) + '</td><td><input type="number" step="0.01" id="v_'+i+'"></td><td><input type="number" id="rpm_'+i+'"></td></tr>');
              }
            </script>
          </tbody>
        </table>
        <button onclick="saveCalibTable()" style="width:100%; margin-top:15px; padding:12px; background:#10b981; border:none; border-radius:8px; color:white; font-weight:bold; cursor:pointer;">GUARDAR TABLA EN FLASH</button>
      </div>

      <div id="panel-hidden-p7">
        <span class="lbl">Constantes del Lazo PI Linealizado</span>
        <div class="config-field">
          <label>Proporcional (Kp)</label>
          <input type="number" id="inputKp" step="0.0001">
        </div>
        <div class="config-field">
          <label>Integral (Ki)</label>
          <input type="number" id="inputKi" step="0.0001">
        </div>
        <button onclick="sendTuneUpdate(); toggleModal(false);" style="width:100%; padding:12px; background:#38bdf8; border:none; border-radius:8px; color:white; font-weight:bold; cursor:pointer;">APLICAR CONSTANTES</button>
      </div>
    </div>
  </div>

</body>
</html>
)rawliteral";