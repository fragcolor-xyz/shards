const puppeteer = require('puppeteer');

// Debug mode: DEBUG_WASM=1 just test-wasm ...
// - Opens DevTools automatically
// - Keeps browser open on error (won't auto-close)
// - Pauses on uncaught exceptions
const DEBUG_MODE = process.env.DEBUG_WASM === '1';

// Custom browser path (for JSPI testing with newer Chrome)
// Set PUPPETEER_EXECUTABLE_PATH to use a specific browser
const BROWSER_PATH = process.env.PUPPETEER_EXECUTABLE_PATH;

(async () => {
  try {
    const launchOptions = {
      args: [
        '--no-sandbox',
        '--disable-web-security',
        '--autoplay-policy=no-user-gesture-required',
        '--auto-accept-camera-and-microphone-capture',
        // Enable JSPI for older Chrome versions (137+)
        '--js-flags=--experimental-wasm-jspi',
        ...(DEBUG_MODE ? ['--auto-open-devtools-for-tabs'] : []),
      ],
      headless: DEBUG_MODE ? false : 'new',
      devtools: DEBUG_MODE,
    };

    // Use specific browser if provided
    if (BROWSER_PATH) {
      console.log('## Using browser:', BROWSER_PATH);
      launchOptions.executablePath = BROWSER_PATH;
    }

    const browser = await puppeteer.launch(launchOptions);

    if (DEBUG_MODE) {
      console.log('## DEBUG MODE: Browser will stay open on errors. Use F12 DevTools to debug.');
    }

    const cmdHandler = {
      'shutdown': () => {
        if (DEBUG_MODE) {
          console.log('## DEBUG MODE: Test finished. Browser staying open for inspection.');
          // Don't close - let user inspect
        } else {
          browser.close();
          process.exit(0);
        }
      },
      'error': (error) => {
        console.error('## Test failed:', error);
        if (DEBUG_MODE) {
          console.log('## DEBUG MODE: Browser staying open for debugging. Close manually when done.');
          // Don't close - let user debug
        } else {
          browser.process().kill();
          process.exit(1);
        }
      }
    };

    const page = await browser.newPage();

    // Disable browser cache to ensure fresh file fetches
    await page.setCacheEnabled(false);

    // Listen for console messages
    page.on('console', msg => {
      if (msg.text().startsWith('<puppeteer>')) {
        // Handle puppeteer command
        const args0 = msg.text().substring('<puppeteer>'.length);
        var args = args0.split(' ');
      var cmdName = args[0];
        if (cmdName in cmdHandler) {
          cmdHandler[cmdName](args.slice(1).join(' '));
        }
      } else {
        console.log('[browser]', msg.text());
      }
    });

    // Listen for page.close() requests
    page.on('close', () => {
      console.log('## Page requested close');
      browser.close();
      process.exit(0);
    });

    // Listen for page errors
    page.on('pageerror', error => {
      console.error('## Page error:', error);
      if (DEBUG_MODE) {
        console.log('## DEBUG MODE: Page error occurred. Browser staying open for debugging.');
      } else {
        browser.close();
        process.exit(1);
      }
    });

    // Listen for worker errors
    page.on('workercreated', worker => {
      worker.on('error', error => {
        console.error('## Worker error:', error);
        if (DEBUG_MODE) {
          console.log('## DEBUG MODE: Worker error occurred. Browser staying open for debugging.');
        } else {
          browser.close();
          process.exit(1);
        }
      });
    });

    // Listen for request failures
    page.on('requestfailed', request => {
      console.error('## Request failed:', request.url(), request.failure().errorText);
      // // Use Windows-friendly process termination
      // browser.process().kill();
      // process.exit(1);
    });

    await page.goto('http://localhost:3000');

    // In debug mode, give user time to open DevTools and set breakpoints
    if (DEBUG_MODE) {
      console.log('## DEBUG MODE: Page loaded. DevTools should be open.');
      console.log('## Set breakpoints in Sources tab, then tests will run automatically.');
    }
  } catch (error) {
    console.error('## Navigation failed:', error);
    if (DEBUG_MODE) {
      console.log('## DEBUG MODE: Navigation failed. Browser staying open for debugging.');
    } else {
      browser.process().kill();
      process.exit(1);
    }
  }
})();