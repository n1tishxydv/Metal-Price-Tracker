const API_BASE = "http://localhost:8080";
const PAGE_LIMIT = 10;

let currentPage = 1;
let totalPages = 1;
let priceChart;

const tableBody = document.getElementById("tableBody");
const trendCards = document.getElementById("trendCards");
const formMessage = document.getElementById("formMessage");
const pageIndicator = document.getElementById("pageIndicator");
const prevPageBtn = document.getElementById("prevPageBtn");
const nextPageBtn = document.getElementById("nextPageBtn");
const downloadCsvBtn = document.getElementById("downloadCsvBtn");
const priceForm = document.getElementById("priceForm");

async function fetchJson(url, options = {}) {
    let response;
    try {
        response = await fetch(url, options);
    } catch (error) {
        throw new Error("Backend is not running on http://localhost:8080.");
    }

    const contentType = response.headers.get("content-type") || "";
    const payload = contentType.includes("application/json")
        ? await response.json()
        : { error: "Unexpected server response." };

    if (!response.ok) {
        throw new Error(payload.error || "Request failed.");
    }

    return payload;
}

function showMessage(message, type) {
    formMessage.textContent = message;
    formMessage.className = `status-message ${type}`;
}

function renderTable(rows) {
    if (!rows.length) {
        tableBody.innerHTML = `
            <tr>
                <td colspan="3">No records available for this page.</td>
            </tr>
        `;
        return;
    }

    tableBody.innerHTML = rows.map((row) => `
        <tr>
            <td>${row.date}</td>
            <td>${row.metal}</td>
            <td>${Number(row.price).toFixed(2)}</td>
        </tr>
    `).join("");
}

function renderTrendCards(analytics) {
    const cards = [analytics.gold, analytics.silver].filter(Boolean);
    trendCards.innerHTML = cards.map((item) => `
        <article class="trend-card ${item.metal}">
            <h3>${item.metal.charAt(0).toUpperCase() + item.metal.slice(1)}</h3>
            <div class="trend-metric">${Number(item.current_price).toFixed(2)}</div>
            <div class="trend-meta">Trend: ${item.trend}</div>
            <div class="trend-meta">Difference: ${Number(item.difference).toFixed(2)}</div>
        </article>
    `).join("");
}

function renderChart(rows) {
    const goldRows = rows
        .filter((row) => row.metal === "gold")
        .slice()
        .reverse();
    const silverRows = rows
        .filter((row) => row.metal === "silver")
        .slice()
        .reverse();

    const labels = [...new Set(
        [...goldRows.map((row) => row.date), ...silverRows.map((row) => row.date)]
    )].sort();

    const goldData = labels.map((label) => {
        const match = goldRows.find((row) => row.date === label);
        return match ? Number(match.price) : null;
    });

    const silverData = labels.map((label) => {
        const match = silverRows.find((row) => row.date === label);
        return match ? Number(match.price) : null;
    });

    const context = document.getElementById("priceChart").getContext("2d");

    if (priceChart) {
        priceChart.destroy();
    }

    priceChart = new Chart(context, {
        type: "line",
        data: {
            labels,
            datasets: [
                {
                    label: "Gold",
                    data: goldData,
                    borderColor: "#d4a017",
                    backgroundColor: "rgba(212, 160, 23, 0.18)",
                    tension: 0.25,
                    fill: false
                },
                {
                    label: "Silver",
                    data: silverData,
                    borderColor: "#8391a1",
                    backgroundColor: "rgba(131, 145, 161, 0.18)",
                    tension: 0.25,
                    fill: false
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    position: "top"
                }
            },
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });
}

async function loadTableAndChart() {
    const payload = await fetchJson(`${API_BASE}/get-data?page=${currentPage}&limit=${PAGE_LIMIT}`);
    totalPages = payload.total_pages || 1;
    pageIndicator.textContent = `Page ${payload.page} of ${totalPages}`;
    prevPageBtn.disabled = currentPage <= 1;
    nextPageBtn.disabled = currentPage >= totalPages;

    renderTable(payload.data);
    renderChart(payload.data);
}

async function loadAnalytics() {
    const analytics = await fetchJson(`${API_BASE}/analytics`);
    renderTrendCards(analytics);
}

async function refreshDashboard() {
    try {
        await loadTableAndChart();
        await loadAnalytics();
    } catch (error) {
        if (tableBody) {
            tableBody.innerHTML = `
                <tr>
                    <td colspan="3">${error.message}</td>
                </tr>
            `;
        }
        if (trendCards) {
            trendCards.innerHTML = `<p>${error.message}</p>`;
        }
    }
}

if (priceForm) {
    priceForm.addEventListener("submit", async (event) => {
        event.preventDefault();

        const payload = {
            date: document.getElementById("dateInput").value,
            metal: document.getElementById("metalInput").value,
            price: Number(document.getElementById("priceInput").value)
        };

        try {
            await fetchJson(`${API_BASE}/add-data`, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify(payload)
            });

            showMessage("Record added successfully.", "success");
            priceForm.reset();
            currentPage = 1;
            await refreshDashboard();
        } catch (error) {
            showMessage(error.message, "error");
        }
    });
}

if (prevPageBtn) {
    prevPageBtn.addEventListener("click", async () => {
        if (currentPage > 1) {
            currentPage -= 1;
            await loadTableAndChart();
        }
    });
}

if (nextPageBtn) {
    nextPageBtn.addEventListener("click", async () => {
        if (currentPage < totalPages) {
            currentPage += 1;
            await loadTableAndChart();
        }
    });
}

if (downloadCsvBtn) {
    downloadCsvBtn.addEventListener("click", async () => {
        try {
            const response = await fetch(`${API_BASE}/download-csv`);
            if (!response.ok) {
                throw new Error("Unable to download CSV.");
            }

            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const link = document.createElement("a");
            link.href = url;
            link.download = "dataset.csv";
            document.body.appendChild(link);
            link.click();
            link.remove();
            window.URL.revokeObjectURL(url);
        } catch (error) {
            showMessage(error.message, "error");
        }
    });
}

if (tableBody && trendCards) {
    refreshDashboard();
}
