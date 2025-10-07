import { expect, test } from '@playwright/test';

test.describe('Authoring flows', () => {
  test('hides the citation preview tooltip after focus leaves the trigger', async ({ page }) => {
    await page.setContent(`
      <style>
        [role="tooltip"] {
          background: #222;
          color: white;
          padding: 4px 8px;
          border-radius: 4px;
          position: absolute;
          top: 32px;
          left: 0;
          display: none;
        }

        [role="tooltip"].visible {
          display: block;
        }
      </style>
      <button id="preview" aria-describedby="cite-preview">Show citation</button>
      <div role="tooltip" id="cite-preview">Preview tooltip</div>
      <script>
        const trigger = document.getElementById('preview');
        const tooltip = document.getElementById('cite-preview');
        trigger.addEventListener('mouseenter', () => {
          tooltip.classList.add('visible');
        });
        trigger.addEventListener('mouseleave', () => {
          setTimeout(() => tooltip.classList.remove('visible'), 150);
        });
        trigger.addEventListener('blur', () => {
          setTimeout(() => tooltip.classList.remove('visible'), 150);
        });
      </script>
    `);

    const trigger = page.getByRole('button', { name: 'Show citation' });
    await trigger.hover();
    await expect(page.getByRole('tooltip')).toBeVisible();

    await page.mouse.move(300, 300);
    await page.waitForTimeout(300);
    await expect(page.getByRole('tooltip')).toBeHidden();
  });

  test('applies container suggestion after asynchronous selection completes', async ({ page }) => {
    await page.setContent(`
      <label for="container">Container ID:</label>
      <input id="container" name="container" aria-label="Container ID:" />
      <button id="pick">Pick suggestion</button>
      <script>
        const input = document.getElementById('container');
        const pick = document.getElementById('pick');
        pick.addEventListener('click', () => {
          setTimeout(() => {
            input.value = 'alpha-beta-42';
            input.dispatchEvent(new Event('input', { bubbles: true }));
            input.dispatchEvent(new Event('change', { bubbles: true }));
          }, 250);
        });
      </script>
    `);

    await page.getByRole('button', { name: 'Pick suggestion' }).click();

    const containerField = page.getByLabel('Container ID:');
    await expect.poll(async () => containerField.inputValue()).toBe('alpha-beta-42');
  });
});
