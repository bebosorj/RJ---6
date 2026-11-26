
import {
    initializeApp
} from "https://www.gstatic.com/firebasejs/12.7.0/firebase-app.js";

import {
    getAuth,
    signInAnonymously
} from "https://www.gstatic.com/firebasejs/12.7.0/firebase-auth.js";

import {
    getDatabase,
    ref,
    onValue,
    set
} from "https://www.gstatic.com/firebasejs/12.7.0/firebase-database.js";

// ==================================================
// RJ FIREBASE CONFIGURATION
// ==================================================

const firebaseConfig = {
    apiKey: "AIzaSyBeAHm65ky0p5mDpSMp6obm3FFcHIF-NCs",
    authDomain: "rjbeboso-5c489.firebaseapp.com",

    databaseURL:
        "https://rjbeboso-5c489-default-rtdb.europe-west1.firebasedatabase.app/",

    projectId: "rjbeboso-5c489",

    storageBucket:
        "rjbeboso-5c489.firebasestorage.app",

    messagingSenderId: "779243980452",

    appId:
        "1:779243980452:web:5d321d762e0d598d0b8831",

    measurementId: "G-FXFKRYPKKH"
};

// ==================================================
// INITIALIZE FIREBASE
// ==================================================

const firebaseApp = initializeApp(firebaseConfig);

const database = getDatabase(firebaseApp);

const auth = getAuth(firebaseApp);

// FIREBASE DATABASE REFERENCES

const dataRef = ref(database, "ESP32_Data");

const relayStatusRef = ref(database, "ESP32_Status");

// ==================================================
// GLOBAL VARIABLES
// ==================================================

let allSensorData = {};

let selectedDate = "";

let sensorChart = null;

let relayStates = {
    LED1: false,
    LED2: false
};

let relayStatusLoaded = false;

let relayPending = {
    LED1: false,
    LED2: false
};

// ==================================================
// DOM ELEMENTS
// ==================================================

const statusElement =
    document.getElementById("firebaseStatus");

const currentTemperature =
    document.getElementById("currentTemperature");

const currentHumidity =
    document.getElementById("currentHumidity");

const graphDate =
    document.getElementById("graphDate");

const historyBody =
    document.getElementById("historyTableBody");

const recordCount =
    document.getElementById("recordCount");

const toggleHistory =
    document.getElementById("toggleHistory");

const historyContent =
    document.getElementById("historyContent");

const relayMessage =
    document.getElementById("relayMessage");

// ==================================================
// FIREBASE STATUS
// ==================================================

function setStatus(text, connected) {

    if (!statusElement) return;

    statusElement.textContent =
        "Firebase Status: " + text;

    statusElement.classList.toggle(
        "status-connected",
        connected
    );

    statusElement.classList.toggle(
        "status-error",
        !connected
    );
}

// ==================================================
// NUMBER HELPERS
// ==================================================

function toNumber(value) {

    const number = Number(value);

    return Number.isFinite(number)
        ? number
        : null;
}

function formatNumber(value) {

    return value === null || value === undefined
        ? "--"
        : Number(value).toFixed(1);
}

// ==================================================
// DATE LIST
// ==================================================

function getDateList(data) {

    return Object.keys(data || {})

        .filter(key =>
            data[key] &&
            typeof data[key] === "object"
        )

        .sort()

        .reverse();
}

// ==================================================
// POPULATE DATE SELECT
// ==================================================

function populateDateSelect() {

    const dates = getDateList(allSensorData);

    graphDate.innerHTML = "";

    if (dates.length === 0) {

        const option = document.createElement("option");

        option.value = "";

        option.textContent = "No dates available";

        graphDate.appendChild(option);

        return;
    }

    dates.forEach(date => {

        const option = document.createElement("option");

        option.value = date;

        option.textContent = date;

        graphDate.appendChild(option);

    });

    if (!dates.includes(selectedDate)) {

        selectedDate = dates[0];

    }

    graphDate.value = selectedDate;
}

// ==================================================
// GET LATEST READING
// ==================================================

function getLatestReading(data) {

    let latest = null;

    const dates = Object.keys(data || {}).sort();

    for (const date of dates) {

        const times = Object.keys(data[date] || {}).sort();

        for (const time of times) {

            const reading = data[date][time];

            if (!reading || typeof reading !== "object") {
                continue;
            }

            const temperature =
                toNumber(reading.temperature);

            const humidity =
                toNumber(reading.humidity);

            if (
                temperature === null &&
                humidity === null
            ) {
                continue;
            }

            latest = {
                date,
                time,
                temperature,
                humidity
            };
        }
    }

    return latest;
}

// ==================================================
// UPDATE CURRENT READINGS
// ==================================================

function updateCurrentReading() {

    const latest = getLatestReading(allSensorData);

    if (!latest) {

        currentTemperature.textContent = "-- °C";

        currentHumidity.textContent = "-- %";

        return;
    }

    currentTemperature.textContent =
        formatNumber(latest.temperature) + " °C";

    currentHumidity.textContent =
        formatNumber(latest.humidity) + " %";
}

// ==================================================
// GET READINGS FOR SELECTED DATE
// ==================================================

function getReadingsForDate(date) {

    const result = [];

    if (!date) return result;

    const dayData = allSensorData[date];

    if (!dayData || typeof dayData !== "object") {
        return result;
    }

    Object.keys(dayData).sort().forEach(time => {

        const reading = dayData[time];

        if (!reading || typeof reading !== "object") {
            return;
        }

        const temperature =
            toNumber(reading.temperature);

        const humidity =
            toNumber(reading.humidity);

        if (
            temperature === null &&
            humidity === null
        ) {
            return;
        }

        result.push({
            time,
            temperature,
            humidity
        });

    });

    return result;
}

// ==================================================
// UPDATE GRAPH
// ==================================================

function updateChart() {

    const readings =
        getReadingsForDate(graphDate.value);

    const canvas =
        document.getElementById("sensorChart");

    if (!canvas) return;

    const labels =
        readings.map(item => item.time);

    const temperatures =
        readings.map(item => item.temperature);

    const humidities =
        readings.map(item => item.humidity);

    if (sensorChart) {

        sensorChart.destroy();

    }

    sensorChart = new Chart(canvas, {

        type: "line",

        data: {

            labels,

            datasets: [

                {
                    label: "Temperature (°C)",

                    data: temperatures,

                    yAxisID: "temperature",

                    borderColor: "#f87171",

                    backgroundColor:
                        "rgba(248,113,113,0.12)",

                    tension: 0.3,

                    borderWidth: 3,

                    pointRadius: 3
                },

                {
                    label: "Humidity (%)",

                    data: humidities,

                    yAxisID: "humidity",

                    borderColor: "#38bdf8",

                    backgroundColor:
                        "rgba(56,189,248,0.12)",

                    tension: 0.3,

                    borderWidth: 3,

                    pointRadius: 3
                }

            ]
        },

        options: {

            responsive: true,

            maintainAspectRatio: false,

            interaction: {

                mode: "index",

                intersect: false

            },

            plugins: {

                legend: {

                    labels: {

                        color: "#cbd5e1"

                    }

                }

            },

            scales: {

                x: {

                    ticks: {

                        color: "#94a3b8"

                    },

                    grid: {

                        color: "#334155"

                    }

                },

                temperature: {

                    type: "linear",

                    position: "left",

                    title: {

                        display: true,

                        text: "Temperature (°C)",

                        color: "#f87171"

                    },

                    ticks: {

                        color: "#f87171"

                    },

                    grid: {

                        color: "#334155"

                    }

                },

                humidity: {

                    type: "linear",

                    position: "right",

                    title: {

                        display: true,

                        text: "Humidity (%)",

                        color: "#38bdf8"

                    },

                    ticks: {

                        color: "#38bdf8"

                    },

                    grid: {

                        drawOnChartArea: false

                    }

                }

            }

        }

    });
}

// ==================================================
// UPDATE HISTORY
// ==================================================

function updateHistory() {

    const readings =
        getReadingsForDate(graphDate.value);

    historyBody.innerHTML = "";

    if (readings.length === 0) {

        historyBody.innerHTML =
            '<tr><td colspan="3">No sensor data available.</td></tr>';

        recordCount.textContent = "0 records";

        return;
    }

    readings.slice().reverse().forEach(reading => {

        const row = document.createElement("tr");

        const timeCell = document.createElement("td");

        const tempCell = document.createElement("td");

        const humCell = document.createElement("td");

        timeCell.textContent = reading.time;

        tempCell.textContent =
            formatNumber(reading.temperature) + " °C";

        humCell.textContent =
            formatNumber(reading.humidity) + " %";

        row.append(timeCell, tempCell, humCell);

        historyBody.appendChild(row);

    });

    recordCount.textContent =
        readings.length +
        (readings.length === 1 ? " record" : " records");
}

// ==================================================
// UPDATE DASHBOARD
// ==================================================

function updateDashboard() {

    const dates = getDateList(allSensorData);

    if (dates.length === 0) {

        currentTemperature.textContent = "-- °C";

        currentHumidity.textContent = "-- %";

        graphDate.innerHTML =
            '<option value="">No dates available</option>';

        historyBody.innerHTML =
            '<tr><td colspan="3">No sensor data available.</td></tr>';

        recordCount.textContent = "0 records";

        if (sensorChart) {

            sensorChart.destroy();

            sensorChart = null;

        }

        return;
    }

    populateDateSelect();

    updateCurrentReading();

    updateChart();

    updateHistory();
}

// ==================================================
// RELAY DISPLAY
// ==================================================

function updateRelayDisplay(relay, isOn) {

    const status = document.getElementById(
        relay === 1 ? "led1Status" : "led2Status"
    );

    const button = document.getElementById(
        relay === 1 ? "led1Button" : "led2Button"
    );

    if (!status || !button) return;

    status.textContent = isOn ? "ON" : "OFF";

    status.className =
        "led-status " + (isOn ? "on" : "off");

    button.textContent =
        isOn ? "Turn OFF" : "Turn ON";
}

// ==================================================
// RELAY STATUS LISTENER
// ==================================================

function startRelayListener() {

    onValue(

        relayStatusRef,

        snapshot => {

            const data = snapshot.val() || {};

            relayStates.LED1 =
                data.LED1 === true;

            relayStates.LED2 =
                data.LED2 === true;

            relayStatusLoaded = true;

            updateRelayDisplay(1, relayStates.LED1);

            updateRelayDisplay(2, relayStates.LED2);

            relayMessage.textContent =
                "Relay status synchronized with ESP32.";

            console.log(
                "ESP32 Relay Status:",
                relayStates
            );

        },

        error => {

            console.error(
                "Relay status error:",
                error
            );

            relayMessage.textContent =
                "Cannot read relay status from Firebase.";

        }

    );
}

// ==================================================
// FIREBASE SENSOR LISTENER
// ==================================================

function startSensorListener() {

    onValue(

        dataRef,

        snapshot => {

            allSensorData = snapshot.val() || {};

            updateDashboard();

            console.log("Sensor data updated.");

        },

        error => {

            console.error(
                "Sensor listener error:",
                error
            );

            setStatus("Connection Error", false);

        }

    );
}

// ==================================================
// FIREBASE CONNECTION
// ==================================================

function startFirebaseListeners() {

    console.log("Connecting to RJ Firebase...");

    setStatus("Connected", true);

    startSensorListener();

    startRelayListener();
}

// ==================================================
// FIREBASE ANONYMOUS LOGIN
// ==================================================

console.log("Signing in to RJ Firebase...");

setStatus("Authenticating...", false);

signInAnonymously(auth)

    .then(() => {

        console.log(
            "Firebase anonymous login successful."
        );

        startFirebaseListeners();

    })

    .catch(error => {

        console.error(
            "Firebase login error:",
            error
        );

        setStatus("Authentication Error", false);

        relayMessage.textContent =
            "Firebase login failed. Check authentication settings.";

    });

// ==================================================
// DATE FILTER
// ==================================================

graphDate.addEventListener("change", function () {

    selectedDate = this.value;

    updateChart();

    updateHistory();

});

// ==================================================
// SHOW / HIDE HISTORY
// ==================================================

toggleHistory.addEventListener("click", function () {

    const hidden =
        historyContent.classList.toggle("hidden");

    toggleHistory.textContent =
        hidden ? "Show History" : "Hide History";

    if (!hidden) {

        updateHistory();

    }

});

// ==================================================
// REMOTE RELAY CONTROL
// ==================================================

async function toggleRelay(relay) {

    const relayName =
        relay === 1 ? "LED1" : "LED2";

    const button = document.getElementById(
        relay === 1 ? "led1Button" : "led2Button"
    );

    if (!button || !relayMessage) return;

    // Check Firebase connection.

    if (!relayStatusLoaded) {

        relayMessage.textContent =
            "Waiting for relay status from ESP32...";

        return;
    }

    // Prevent repeated clicks.

    if (relayPending[relayName]) {

        return;

    }

    relayPending[relayName] = true;

    button.disabled = true;

    relayMessage.textContent =
        "Sending command to ESP32...";

    try {

        // Get opposite of latest confirmed status.

        const nextState =
            !relayStates[relayName];

        // Send command to Firebase.

        await set(

            ref(
                database,
                "ESP32_Control/" + relayName
            ),

            nextState

        );

        console.log(
            relayName + " command sent:",
            nextState ? "ON" : "OFF"
        );

        relayMessage.textContent =
            "Command sent! Waiting for ESP32 response...";

    }

    catch (error) {

        console.error(
            "Relay command error:",
            error
        );

        relayMessage.textContent =
            "Failed to send command. Check Firebase connection.";

    }

    finally {

        relayPending[relayName] = false;

        button.disabled = false;

    }
}

// ==================================================
// RELAY BUTTON EVENTS
// ==================================================

document.getElementById("led1Button")
    .addEventListener("click", () => {

        toggleRelay(1);

    });

document.getElementById("led2Button")
    .addEventListener("click", () => {

        toggleRelay(2);

    });

// ==================================================
// GLOBAL FUNCTION
// ==================================================

window.toggleRelay = toggleRelay;

console.log("RJ DHT11 Dashboard Initialized.");
