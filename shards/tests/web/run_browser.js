const puppeteer = require('puppeteer');


(async () => {
  try {
    const browser = await puppeteer.launch({
      args: [
        '--no-sandbox',
        '--disable-web-security',
        '--autoplay-policy=no-user-gesture-required',
        '--auto-accept-camera-and-microphone-capture'
      ],
      headless: false,
    });

    const cmdHandler = {
      'shutdown': () => {
        browser.close();
        process.exit(0);
      },
      'error': (error) => {
        console.error('## Test failed:', error);
        browser.process().kill();
        process.exit(1);
      }
    };

    const page = await browser.newPage();

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
      browser.close();
      process.exit(1);
    });

    // Listen for worker errors
    page.on('workercreated', worker => {
      worker.on('error', error => {
        console.error('## Worker error:', error);
        browser.close();
        process.exit(1);
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
  } catch (error) {
    console.error('## Navigation failed:', error);
    browser.process().kill();
    process.exit(1);
  }
})();