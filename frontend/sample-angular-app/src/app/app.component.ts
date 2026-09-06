import {AfterViewInit, Component} from '@angular/core';

import {SystemInfo} from './app.model';
import {NxBridgeService} from './nx-bridge.service';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent implements AfterViewInit {
  sumResult: number | undefined;
  systemInfo: SystemInfo | undefined;
  error = '';
  lifecycle = 'active';
  calculating = false;

  constructor(readonly bridge: NxBridgeService) {}

  ngAfterViewInit(): void {
    if (!this.bridge.available) {
      return;
    }

    this.bridge.onNotification(message => {
      if (message.method === 'app.lifecycle') {
        const params = message.params as {state?: string};
        this.lifecycle = params.state ?? 'active';
      }
    });
    void this.start();
  }

  async calculate(first: string, second: string): Promise<void> {
    const a = Number(first);
    const b = Number(second);
    if (!Number.isFinite(a) || !Number.isFinite(b)) {
      this.error = 'Enter two valid numbers.';
      return;
    }

    this.error = '';
    this.calculating = true;
    try {
      this.sumResult = await this.bridge.call<number>('math.sum', {a, b});
    } catch (error) {
      this.error = error instanceof Error ? error.message : String(error);
    } finally {
      this.calculating = false;
    }
  }

  private async loadSystemInfo(): Promise<void> {
    try {
      this.systemInfo = await this.bridge.call<SystemInfo>('system.info');
    } catch (error) {
      this.error = error instanceof Error ? error.message : String(error);
    }
  }

  private async start(): Promise<void> {
    try {
      await this.bridge.connect('angular');
      await this.loadSystemInfo();
      await this.calculate('3', '4');
    } catch (error) {
      this.error = error instanceof Error ? error.message : String(error);
    }
  }
}
