import { enableProdMode } from '@angular/core';
import { platformBrowserDynamic } from '@angular/platform-browser-dynamic';

import { AppModule } from './app/app.module';
import { environment } from './environments/environment';

if (environment.production) {
  enableProdMode();
}

platformBrowserDynamic().bootstrapModule(AppModule)
  .catch(error => {
    window.nxSpaBootError = true;
    const message = document.getElementById('nx-boot-message');
    if (message) {
      const detail = error instanceof Error ? error.stack ?? error.message : String(error);
      message.textContent = `Angular bootstrap error: ${detail}`;
    }
    console.error(error);
  });
