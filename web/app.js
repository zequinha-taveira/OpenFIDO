/**
 * OpenFIDO Web Manager
 * Handles WebUSB connection and UI interactions
 */

const connectBtn = document.getElementById('connectBtn');
const dashboard = document.getElementById('dashboard');
const connectionStatus = document.getElementById('connectionStatus');
const statusDot = connectionStatus.querySelector('.status-dot');
const statusText = connectionStatus.querySelector('.status-text');
const fwVersionSpan = document.getElementById('fwVersion');
const serialNumberSpan = document.getElementById('serialNumber');

let device = null;

// Mock Data for Demo
const MOCK_DEVICE = {
    fwVersion: '1.0.0',
    serial: 'OPENFIDO-DEMO-01'
};

connectBtn.addEventListener('click', async () => {
    try {
        // In a real implementation, we would request a WebUSB device here
        // device = await navigator.usb.requestDevice({ filters: [{ vendorId: 0xCAFE }] });

        // Simulating connection for UI demo
        simulateConnection();
    } catch (error) {
        console.error('Connection failed:', error);
        alert('Failed to connect to device. Make sure it is plugged in.');
    }
});

function simulateConnection() {
    connectBtn.textContent = 'Connecting...';
    connectBtn.disabled = true;

    setTimeout(() => {
        // Update UI to Connected State
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';

        // Hide Hero Button, Show Dashboard
        connectBtn.style.display = 'none';
        dashboard.classList.remove('hidden');

        // Populate Data
        fwVersionSpan.textContent = MOCK_DEVICE.fwVersion;
        serialNumberSpan.textContent = MOCK_DEVICE.serial;

        // Animation effect
        dashboard.style.opacity = 0;
        dashboard.style.transform = 'translateY(20px)';
        requestAnimationFrame(() => {
            dashboard.style.transition = 'all 0.5s ease';
            dashboard.style.opacity = 1;
            dashboard.style.transform = 'translateY(0)';
        });

    }, 1500);
}

// Reset Button Handler
document.getElementById('resetBtn').addEventListener('click', () => {
    if (confirm('Are you sure you want to reset the device? This will erase all credentials.')) {
        alert('Reset command sent to device.');
    }
});

// PIN Toggle Handler
document.getElementById('pinToggle').addEventListener('change', (e) => {
    console.log('PIN Protection:', e.target.checked ? 'Enabled' : 'Disabled');
});

/* ========== Firmware Update Logic (Web Serial) ========== */

const firmwareInput = document.getElementById('firmwareInput');
const fileNameSpan = document.getElementById('fileName');
const flashBtn = document.getElementById('flashBtn');
const progressContainer = document.getElementById('progressContainer');
const progressFill = document.getElementById('progressFill');
const progressPercent = document.getElementById('progressPercent');
const progressStatus = document.getElementById('progressStatus');

let selectedFile = null;

// File Selection
firmwareInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
        selectedFile = e.target.files[0];
        fileNameSpan.textContent = selectedFile.name;
        flashBtn.disabled = false;
    }
});

// Flash Button
flashBtn.addEventListener('click', async () => {
    if (!selectedFile) return;

    if (!('serial' in navigator)) {
        alert('Web Serial API not supported in this browser. Please use Chrome or Edge.');
        return;
    }

    try {
        // Request Serial Port
        const port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 });

        // UI Update
        flashBtn.disabled = true;
        progressContainer.classList.remove('hidden');
        progressStatus.textContent = 'Erasing flash...';
        progressFill.style.width = '0%';
        progressPercent.textContent = '0%';

        // Read File
        const buffer = await selectedFile.arrayBuffer();
        const data = new Uint8Array(buffer);

        // Simulate Flashing Process
        // In a real app, we would write chunks to the writer:
        // const writer = port.writable.getWriter();
        // await writer.write(chunk);

        await simulateFlashing(data.length);

        // Success
        progressStatus.textContent = 'Update Complete!';
        progressFill.style.width = '100%';
        progressPercent.textContent = '100%';
        alert('Firmware updated successfully! Device is rebooting.');

        await port.close();

    } catch (error) {
        console.error('Flashing failed:', error);
        alert('Flashing failed: ' + error.message);
        progressStatus.textContent = 'Error';
    } finally {
        flashBtn.disabled = false;
    }
});

async function simulateFlashing(totalBytes) {
    const chunkSize = 1024;
    const totalChunks = Math.ceil(totalBytes / chunkSize);
    let currentChunk = 0;

    return new Promise((resolve) => {
        const interval = setInterval(() => {
            currentChunk++;
            const percent = Math.min(Math.round((currentChunk / totalChunks) * 100), 100);

            progressFill.style.width = `${percent}%`;
            progressPercent.textContent = `${percent}%`;
            progressStatus.textContent = percent < 100 ? 'Writing firmware...' : 'Verifying...';

            if (currentChunk >= totalChunks) {
                clearInterval(interval);
                resolve();
            }
        }, 50); // Speed of simulation
    });
}
