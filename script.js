// BridgeBrain Frontend Logic

let loadChart, vibrationChart, tiltChart;
const maxPoints = 40;
const seenEvents = new Set();
const ALERT_LIMIT = 9000; // 9 tons

function initCharts() {
  const loadCtx = document.getElementById("loadChart").getContext("2d");
  const vibCtx  = document.getElementById("vibrationChart").getContext("2d");
  const tiltCtx = document.getElementById("tiltChart").getContext("2d");

  const cfg = (label, color) => ({
    type: "line",
    data: { labels: [], datasets: [{ label, data: [], borderColor: color, borderWidth: 2, pointRadius: 2 }] },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      scales: { y: { beginAtZero: true } }
    }
  });

  loadChart      = new Chart(loadCtx, cfg("Load (kg)", "#FFD54F"));
  vibrationChart = new Chart(vibCtx,  cfg("Vibration (Hz)", "#03A9F4"));
  tiltChart      = new Chart(tiltCtx, cfg("Tilt (°)", "#4CAF50"));
}

function pushPoint(chart, label, value) {
  chart.data.labels.push(label);
  chart.data.datasets[0].data.push(value);
  if (chart.data.labels.length > maxPoints) {
    chart.data.labels.shift();
    chart.data.datasets[0].data.shift();
  }
  chart.update("none");
}

function updateCharts(last) {
  const time = last.Time || "";
  pushPoint(loadChart,      time, Number(last.totalLoad || 0));
  pushPoint(vibrationChart, time, Number(last.vibration || 0));
  pushPoint(tiltChart,      time, Number(last.tilt || 0));
}

function updateUI(last) {
  const count = Number(last.vehicleCount || 0);
  const load  = Number(last.totalLoad || 0);
  const vib   = Number(last.vibration || 0);
  const tilt  = Number(last.tilt || 0);

  const vc = document.getElementById("vehicle-count-box");
  if (vc) vc.innerText = count;

  const vibEl = document.getElementById("vibration-text");
  if (vibEl) vibEl.innerText = vib.toFixed(3) + " Hz";

  const tiltEl = document.getElementById("tilt-text");
  if (tiltEl) tiltEl.innerText = tilt.toFixed(2) + "°";

  const statusEl = document.getElementById("status-text");
  if (statusEl) {
    if (load > ALERT_LIMIT) {
      statusEl.innerText = "ALERT";
      statusEl.style.color = "red";
    } else {
      statusEl.innerText = "Stable";
      statusEl.style.color = "lime";
    }
  }
}

function updateHistory(last) {
  const key = (last.Time || "") + (last.event || "") + (last.vehicleWeight || "");
  if (seenEvents.has(key)) return;
  seenEvents.add(key);

  const box = document.getElementById("vehicle-history");
  if (!box) return;

  const div = document.createElement("div");
  div.className = "history-item";

  const load = Number(last.totalLoad || 0);
  const isAlert = load > ALERT_LIMIT;

  div.innerHTML = `
    ${last.Time || ""} — 
    <b>${(last.event || "").toUpperCase()}</b> — 
    Vehicle: ${last.vehicleWeight} kg — 
    Total Load: ${load} kg
    ${isAlert ? '<span style="color:red;font-weight:bold;">  [ALERT]</span>' : ""}
  `;
  box.prepend(div);

  while (box.children.length > 15) box.removeChild(box.lastChild);
}

function updateAlertScreen(last) {
  const load = Number(last.totalLoad || 0);
  const overlay = document.getElementById("danger-overlay");
  if (!overlay) return;

  if (load > ALERT_LIMIT) {
    document.body.classList.add("danger-mode");
    overlay.style.display = "flex";
  } else {
    document.body.classList.remove("danger-mode");
    overlay.style.display = "none";
  }
}

async function poll() {
  try {
    const res = await fetch("/api/latest-readings");
    if (res.ok) {
      const data = await res.json();
      if (Array.isArray(data) && data.length > 0) {
        const last = data[data.length - 1]; // latest reading
        if (last.event) {   // ignore empty boot lines
          updateCharts(last);
          updateUI(last);
          updateHistory(last);
          updateAlertScreen(last);
        }
      }
    }
  } catch (e) {
    // network / backend off – ignore
  }
  setTimeout(poll, 1000);
}

window.addEventListener("load", () => {
  initCharts();
  poll();
});
