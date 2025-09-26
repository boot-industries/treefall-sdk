import { bootstrapFromCatalog } from './runtime/packs';

function ensureRoot(): HTMLElement {
  const root = document.getElementById('app');
  if (!root) {
    throw new Error('Wordbird: #app container missing');
  }
  return root;
}

function attachServiceWorker(): void {
  const script = document.createElement('script');
  script.type = 'module';
  script.src = '/sw-register.js';
  script.onerror = (event) => {
    console.error('Wordbird: failed to load sw-register.js', event);
  };
  document.head?.appendChild(script);
}

function renderStatus(message: string, retry?: () => void): void {
  const root = ensureRoot();
  root.innerHTML = '';
  const container = document.createElement('div');
  container.className = 'wordbird-status';

  const text = document.createElement('p');
  text.textContent = message;
  container.appendChild(text);

  if (retry) {
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = 'Retry';
    button.addEventListener('click', () => {
      retry();
    });
    container.appendChild(button);
  }

  root.appendChild(container);
}

async function main(): Promise<void> {
  try {
    const result = await bootstrapFromCatalog();
    const root = ensureRoot();
    root.innerHTML = '';

    const info = document.createElement('pre');
    info.textContent = JSON.stringify(result, null, 2);
    root.appendChild(info);
  } catch (error) {
    console.error('Wordbird: bootstrap failed', error);
    const message =
      error instanceof Error ? error.message : typeof error === 'string' ? error : 'unknown error';
    renderStatus(`Unable to download packs (${message}).`, () => {
      void main();
    });
  }
}

attachServiceWorker();

void main();
