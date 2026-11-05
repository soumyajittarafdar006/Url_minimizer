/**
 * TinyURL – Frontend Client Script
 * Communicates with the lightweight Python HTTP bridge and C Backend
 */

document.addEventListener('DOMContentLoaded', () => {
  // ==========================================
  // State & Elements
  // ==========================================
  let currentSearchMode = 'code'; // 'code' or 'url'
  let pendingDeleteCode = null;
  let statsBucketData = [];

  // Navigation
  const navLinks = document.querySelectorAll('.nav-link');
  const sections = document.querySelectorAll('.page-section');

  // Shortener elements
  const shortenForm = document.getElementById('shortenForm');
  const longUrlInput = document.getElementById('longUrlInput');
  const shortenBtn = document.getElementById('shortenBtn');
  const shortenError = document.getElementById('shortenError');
  const resultCard = document.getElementById('resultCard');
  const resultOriginalUrl = document.getElementById('resultOriginalUrl');
  const resultShortUrl = document.getElementById('resultShortUrl');
  const resultClicks = document.getElementById('resultClicks');
  const copyResultBtn = document.getElementById('copyResultBtn');
  const openResultBtn = document.getElementById('openResultBtn');

  // My URLs elements
  const refreshListBtn = document.getElementById('refreshListBtn');
  const urlsTable = document.getElementById('urlsTable');
  const urlsTableBody = document.getElementById('urlsTableBody');
  const emptyListState = document.getElementById('emptyListState');
  const emptyShortenLink = document.getElementById('emptyShortenLink');

  // Search elements
  const searchModeCodeBtn = document.getElementById('searchModeCodeBtn');
  const searchModeUrlBtn = document.getElementById('searchModeUrlBtn');
  const searchForm = document.getElementById('searchForm');
  const searchInput = document.getElementById('searchInput');
  const searchBtn = document.getElementById('searchBtn');
  const searchMessage = document.getElementById('searchMessage');
  const searchResultCard = document.getElementById('searchResultCard');
  const searchShortCode = document.getElementById('searchShortCode');
  const searchOriginalUrl = document.getElementById('searchOriginalUrl');
  const searchShortUrl = document.getElementById('searchShortUrl');
  const searchClicks = document.getElementById('searchClicks');
  const copySearchBtn = document.getElementById('copySearchBtn');
  const openSearchBtn = document.getElementById('openSearchBtn');

  // Statistics elements
  const refreshStatsBtn = document.getElementById('refreshStatsBtn');
  const statTotalRecords = document.getElementById('statTotalRecords');
  const statOccupiedBuckets = document.getElementById('statOccupiedBuckets');
  const statCollidedBuckets = document.getElementById('statCollidedBuckets');
  const statCollisions = document.getElementById('statCollisions');
  const statMaxChain = document.getElementById('statMaxChain');
  const statLoadFactor = document.getElementById('statLoadFactor');
  const bucketGrid = document.getElementById('bucketGrid');
  const bucketInspector = document.getElementById('bucketInspector');

  // Modal elements
  const deleteModal = document.getElementById('deleteModal');
  const modalBackdrop = document.getElementById('modalBackdrop');
  const modalShortCode = document.getElementById('modalShortCode');
  const modalOriginalUrl = document.getElementById('modalOriginalUrl');
  const cancelDeleteBtn = document.getElementById('cancelDeleteBtn');
  const confirmDeleteBtn = document.getElementById('confirmDeleteBtn');

  // Toast Container
  const toastContainer = document.getElementById('toastContainer');

  // ==========================================
  // Helper: Toast Notifications
  // ==========================================
  function showToast(message, type = 'success', duration = 3000) {
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.textContent = message;
    toastContainer.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateY(10px)';
      toast.style.transition = 'all 0.25s ease';
      setTimeout(() => toast.remove(), 250);
    }, duration);
  }

  // ==========================================
  // Helper: Copy to Clipboard
  // ==========================================
  async function copyToClipboard(text, btnElement) {
    try {
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
      } else {
        const temp = document.createElement('textarea');
        temp.value = text;
        temp.style.position = 'fixed';
        temp.style.opacity = '0';
        document.body.appendChild(temp);
        temp.focus();
        temp.select();
        document.execCommand('copy');
        document.body.removeChild(temp);
      }
      showToast('Short URL copied to clipboard!', 'success');
      if (btnElement) {
        const origText = btnElement.innerHTML;
        btnElement.innerHTML = `
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
            <polyline points="20 6 9 17 4 12"></polyline>
          </svg> Copied!
        `;
        setTimeout(() => { btnElement.innerHTML = origText; }, 2000);
      }
    } catch (err) {
      showToast('Could not copy link to clipboard.', 'error');
    }
  }

  // ==========================================
  // Navigation & View Routing
  // ==========================================
  function switchSection(sectionId) {
    sections.forEach(sec => {
      if (sec.id === sectionId) {
        sec.classList.add('active');
      } else {
        sec.classList.remove('active');
      }
    });

    navLinks.forEach(link => {
      if (link.dataset.section === sectionId) {
        link.classList.add('active');
      } else {
        link.classList.remove('active');
      }
    });

    window.scrollTo({ top: 0, behavior: 'smooth' });

    if (sectionId === 'my-urls') {
      loadUrlsTable();
    } else if (sectionId === 'statistics') {
      loadStatistics();
    }
  }

  navLinks.forEach(link => {
    link.addEventListener('click', (e) => {
      e.preventDefault();
      const target = link.dataset.section;
      window.location.hash = target;
      switchSection(target);
    });
  });

  if (emptyShortenLink) {
    emptyShortenLink.addEventListener('click', (e) => {
      e.preventDefault();
      window.location.hash = 'home';
      switchSection('home');
      longUrlInput.focus();
    });
  }

  window.addEventListener('hashchange', () => {
    const hash = window.location.hash.replace('#', '') || 'home';
    if (document.getElementById(hash)) {
      switchSection(hash);
    }
  });

  // Initial routing based on URL hash
  const initialHash = window.location.hash.replace('#', '') || 'home';
  if (document.getElementById(initialHash)) {
    switchSection(initialHash);
  }

  // ==========================================
  // Section 3: Shorten URL
  // ==========================================
  shortenForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const url = longUrlInput.value.trim();

    shortenError.classList.add('hidden');
    shortenError.textContent = '';
    resultCard.classList.add('hidden');

    if (!url) {
      shortenError.textContent = 'Please enter a URL to shorten.';
      shortenError.classList.remove('hidden');
      longUrlInput.focus();
      return;
    }

    // Set loading state
    shortenBtn.disabled = true;
    shortenBtn.querySelector('.btn-text').textContent = 'Shortening...';
    shortenBtn.querySelector('.btn-spinner').classList.remove('hidden');

    try {
      const response = await fetch('/api/shorten', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url: url })
      });

      const data = await response.json();

      if (data.success) {
        const fullShortUrl = `${window.location.origin}/r/${data.shortCode}`;
        
        resultOriginalUrl.textContent = data.originalURL;
        resultOriginalUrl.href = data.originalURL;
        resultShortUrl.value = fullShortUrl;
        resultClicks.textContent = `${data.clickCount || 0} Clicks`;
        openResultBtn.href = `/r/${data.shortCode}`;

        resultCard.classList.remove('hidden');
        showToast('URL shortened successfully!', 'success');
        longUrlInput.value = '';

        // Pre-refresh table & statistics
        loadUrlsTable();
        loadStatistics();
      } else {
        shortenError.textContent = data.error || 'Failed to shorten URL.';
        shortenError.classList.remove('hidden');
      }
    } catch (err) {
      shortenError.textContent = 'Server communication error. Please check your network or server.';
      shortenError.classList.remove('hidden');
    } finally {
      shortenBtn.disabled = false;
      shortenBtn.querySelector('.btn-text').textContent = 'Shorten URL';
      shortenBtn.querySelector('.btn-spinner').classList.add('hidden');
    }
  });

  copyResultBtn.addEventListener('click', () => {
    copyToClipboard(resultShortUrl.value, copyResultBtn);
  });

  // ==========================================
  // Section 4: My URLs Table
  // ==========================================
  async function loadUrlsTable() {
    refreshListBtn.disabled = true;
    const refreshIcon = refreshListBtn.querySelector('.refresh-icon');
    if (refreshIcon) refreshIcon.style.animation = 'spin 0.6s linear infinite';

    try {
      const res = await fetch('/api/urls');
      const data = await res.json();

      urlsTableBody.innerHTML = '';

      if (data.success && data.records && data.records.length > 0) {
        emptyListState.classList.add('hidden');
        urlsTable.classList.remove('hidden');

        data.records.forEach((rec, idx) => {
          const tr = document.createElement('tr');
          const fullShortUrl = `${window.location.origin}/r/${rec.shortCode}`;

          tr.innerHTML = `
            <td>${rec.serial || idx + 1}</td>
            <td><code class="code-badge-inline">${rec.shortCode}</code></td>
            <td>
              <a href="${rec.originalURL}" target="_blank" rel="noopener noreferrer" class="link-truncate" title="${rec.originalURL}">
                ${rec.originalURL}
              </a>
            </td>
            <td class="text-center font-mono"><strong>${rec.clickCount}</strong></td>
            <td class="text-right">
              <div class="table-actions">
                <button type="button" class="btn btn-secondary btn-sm copy-table-btn" data-url="${fullShortUrl}" title="Copy short URL">
                  Copy
                </button>
                <a href="/r/${rec.shortCode}" target="_blank" rel="noopener noreferrer" class="btn btn-secondary btn-sm" title="Resolve & open URL">
                  Open
                </a>
                <button type="button" class="btn btn-danger-ghost btn-sm delete-table-btn" data-code="${rec.shortCode}" data-url="${rec.originalURL}" title="Delete record">
                  Delete
                </button>
              </div>
            </td>
          `;
          urlsTableBody.appendChild(tr);
        });

        // Wire Copy buttons in table
        document.querySelectorAll('.copy-table-btn').forEach(btn => {
          btn.addEventListener('click', () => {
            copyToClipboard(btn.dataset.url, btn);
          });
        });

        // Wire Delete buttons in table
        document.querySelectorAll('.delete-table-btn').forEach(btn => {
          btn.addEventListener('click', () => {
            openDeleteModal(btn.dataset.code, btn.dataset.url);
          });
        });
      } else {
        urlsTable.classList.add('hidden');
        emptyListState.classList.remove('hidden');
      }
    } catch (err) {
      showToast('Failed to load stored URLs.', 'error');
    } finally {
      refreshListBtn.disabled = false;
      if (refreshIcon) refreshIcon.style.animation = '';
    }
  }

  refreshListBtn.addEventListener('click', () => {
    loadUrlsTable();
    showToast('URL list refreshed.', 'info');
  });

  // ==========================================
  // Section 4 & 9: Deletion Confirmation Modal
  // ==========================================
  function openDeleteModal(shortCode, originalUrl) {
    pendingDeleteCode = shortCode;
    modalShortCode.textContent = shortCode;
    modalOriginalUrl.textContent = originalUrl;
    deleteModal.classList.remove('hidden');
  }

  function closeDeleteModal() {
    pendingDeleteCode = null;
    deleteModal.classList.add('hidden');
  }

  cancelDeleteBtn.addEventListener('click', closeDeleteModal);
  modalBackdrop.addEventListener('click', closeDeleteModal);

  confirmDeleteBtn.addEventListener('click', async () => {
    if (!pendingDeleteCode) return;

    confirmDeleteBtn.disabled = true;
    confirmDeleteBtn.textContent = 'Deleting...';

    try {
      const res = await fetch('/api/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shortCode: pendingDeleteCode })
      });
      const data = await res.json();

      if (data.success) {
        showToast(`Record '${pendingDeleteCode}' deleted successfully.`, 'success');
        closeDeleteModal();
        loadUrlsTable();
        loadStatistics();
      } else {
        showToast(data.error || 'Failed to delete record.', 'error');
      }
    } catch (err) {
      showToast('Network error while deleting record.', 'error');
    } finally {
      confirmDeleteBtn.disabled = false;
      confirmDeleteBtn.textContent = 'Delete Record';
    }
  });

  // ==========================================
  // Section 5: Search Functionality
  // ==========================================
  searchModeCodeBtn.addEventListener('click', () => {
    currentSearchMode = 'code';
    searchModeCodeBtn.classList.add('active');
    searchModeUrlBtn.classList.remove('active');
    searchInput.placeholder = 'Enter short code to search (e.g. 1)...';
    searchInput.focus();
  });

  searchModeUrlBtn.addEventListener('click', () => {
    currentSearchMode = 'url';
    searchModeUrlBtn.classList.add('active');
    searchModeCodeBtn.classList.remove('active');
    searchInput.placeholder = 'Enter full original URL to search (e.g. https://...)...';
    searchInput.focus();
  });

  searchForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const query = searchInput.value.trim();

    searchMessage.classList.add('hidden');
    searchResultCard.classList.add('hidden');

    if (!query) {
      searchMessage.textContent = 'Please enter a search query.';
      searchMessage.className = 'alert-message alert-error';
      searchMessage.classList.remove('hidden');
      return;
    }

    searchBtn.disabled = true;
    searchBtn.querySelector('.btn-text').textContent = 'Searching...';
    searchBtn.querySelector('.btn-spinner').classList.remove('hidden');

    try {
      const res = await fetch('/api/search', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: currentSearchMode, query: query })
      });

      const data = await res.json();

      if (data.success && data.record) {
        const rec = data.record;
        const fullShortUrl = `${window.location.origin}/r/${rec.shortCode}`;

        searchShortCode.textContent = rec.shortCode;
        searchOriginalUrl.textContent = rec.originalURL;
        searchOriginalUrl.href = rec.originalURL;
        searchShortUrl.value = fullShortUrl;
        searchClicks.textContent = `${rec.clickCount} Clicks`;
        openSearchBtn.href = `/r/${rec.shortCode}`;

        searchResultCard.classList.remove('hidden');
      } else {
        searchMessage.textContent = 'URL not found in hash table.';
        searchMessage.className = 'alert-message alert-error';
        searchMessage.classList.remove('hidden');
      }
    } catch (err) {
      searchMessage.textContent = 'Error connecting to C backend search service.';
      searchMessage.className = 'alert-message alert-error';
      searchMessage.classList.remove('hidden');
    } finally {
      searchBtn.disabled = false;
      searchBtn.querySelector('.btn-text').textContent = 'Search';
      searchBtn.querySelector('.btn-spinner').classList.add('hidden');
    }
  });

  copySearchBtn.addEventListener('click', () => {
    copyToClipboard(searchShortUrl.value, copySearchBtn);
  });

  // ==========================================
  // Section 6: Statistics & Bucket Visualizer
  // ==========================================
  async function loadStatistics() {
    refreshStatsBtn.disabled = true;
    const icon = refreshStatsBtn.querySelector('.refresh-icon');
    if (icon) icon.style.animation = 'spin 0.6s linear infinite';

    try {
      const res = await fetch('/api/stats');
      const data = await res.json();

      if (data.success) {
        statTotalRecords.textContent = data.totalRecords;
        statOccupiedBuckets.textContent = data.usedBuckets;
        statCollidedBuckets.textContent = data.collidedBuckets;
        statCollisions.textContent = data.collisions;
        statMaxChain.textContent = data.maxChainLength;
        statLoadFactor.textContent = Number(data.loadFactor).toFixed(4);

        statsBucketData = data.buckets || [];
        renderBucketGrid(statsBucketData);
      }
    } catch (err) {
      showToast('Could not load statistics from C backend.', 'error');
    } finally {
      refreshStatsBtn.disabled = false;
      if (icon) icon.style.animation = '';
    }
  }

  function renderBucketGrid(buckets) {
    bucketGrid.innerHTML = '';
    
    // Ensure we render all 101 buckets [0..100]
    for (let i = 0; i < 101; i++) {
      const bInfo = buckets.find(b => b.bucket === i) || { bucket: i, count: 0, codes: [] };
      const cell = document.createElement('div');
      
      let stateClass = 'empty';
      if (bInfo.count === 1) stateClass = 'occupied';
      else if (bInfo.count > 1) stateClass = 'collided';

      cell.className = `bucket-cell ${stateClass}`;
      cell.textContent = i;
      cell.title = `Bucket ${i}: ${bInfo.count} record(s)`;
      
      cell.addEventListener('click', () => {
        inspectBucket(bInfo);
      });

      bucketGrid.appendChild(cell);
    }
  }

  function inspectBucket(bInfo) {
    if (!bInfo || bInfo.count === 0) {
      bucketInspector.innerHTML = `
        <strong>Bucket ${bInfo.bucket}:</strong> Empty (Chain Length: 0). No hash collisions.
      `;
      return;
    }

    const codesList = bInfo.codes && bInfo.codes.length > 0 
      ? bInfo.codes.map(c => `<code class="code-badge-inline">${c}</code>`).join(' &rarr; ')
      : 'None';

    const collisionText = bInfo.count > 1 
      ? `<span style="color: var(--color-danger); font-weight: 600;">(Collision: ${bInfo.count - 1} collided items)</span>`
      : '<span style="color: var(--color-success); font-weight: 600;">(No collision)</span>';

    bucketInspector.innerHTML = `
      <div><strong>Bucket Index [${bInfo.bucket}]:</strong> Chain Length: <strong>${bInfo.count}</strong> ${collisionText}</div>
      <div style="margin-top: 0.35rem;">
        <strong>Singly Linked List Chain:</strong> Head &rarr; ${codesList} &rarr; <code>NULL</code>
      </div>
    `;
  }

  refreshStatsBtn.addEventListener('click', () => {
    loadStatistics();
    showToast('Statistics refreshed from C backend.', 'info');
  });

  // Load initial data
  loadUrlsTable();
  loadStatistics();
});
