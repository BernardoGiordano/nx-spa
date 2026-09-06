import {ApplicationRef, Injectable, Injector, NgZone} from '@angular/core';

interface NxMessageEvent {
  data: string;
}

interface NxApi {
  addEventListener(type: 'message', listener: (event: NxMessageEvent) => void): void;
  sendMessage(message: string): boolean;
}

interface RpcError {
  code: number;
  message: string;
}

interface RpcMessage {
  id?: number;
  method?: string;
  params?: unknown;
  result?: unknown;
  error?: RpcError;
}

interface PendingRequest {
  resolve: (value: unknown) => void;
  reject: (reason: Error) => void;
  timeout: number;
}

declare global {
  interface Window {
    nx?: NxApi;
    nxSpaBootError: boolean;
  }
}

@Injectable({providedIn: 'root'})
export class NxBridgeService {
  private readonly nx = window.nx;
  private readonly pending = new Map<number, PendingRequest>();
  private readonly notificationListeners = new Set<(message: RpcMessage) => void>();
  private readyPromise: Promise<void> | undefined;
  private readyResolve: (() => void) | undefined;
  private readyReject: ((reason: Error) => void) | undefined;
  private readyTimeout: number | undefined;
  private bridgeConnected = false;
  private nextId = 1;

  constructor(private readonly zone: NgZone, private readonly injector: Injector) {
    this.nx?.addEventListener('message', event => {
      // window.nx is a host object, not a DOM EventTarget, so Zone.js never
      // patched this callback. Re-enter the zone and force a render pass once
      // the resolved promise continuations have drained.
      this.zone.run(() => {
        if (this.handleMessage(event.data)) {
          window.setTimeout(() => this.renderNow(), 0);
        }
      });
    });
  }

  get available(): boolean {
    return this.nx !== undefined;
  }

  get connected(): boolean {
    return this.bridgeConnected;
  }

  connect(framework: string): Promise<void> {
    if (!this.nx) {
      return Promise.reject(new Error('window.nx is only available in the Switch web applet'));
    }
    if (this.bridgeConnected) {
      return Promise.resolve();
    }
    if (this.readyPromise) {
      return this.readyPromise;
    }

    this.readyPromise = new Promise<void>((resolve, reject) => {
      this.readyResolve = resolve;
      this.readyReject = reject;
      this.readyTimeout = window.setTimeout(() => {
        this.failReady(new Error('Native bridge readiness timed out'));
      }, 10000);

      const payload = JSON.stringify({
        jsonrpc: '2.0',
        method: 'app.ready',
        params: {framework}
      });
      this.send(payload, () => {
        this.failReady(new Error('Native bridge readiness queue stayed full'));
      });
    });
    return this.readyPromise;
  }

  notify(method: string, params: unknown = {}): void {
    this.send(JSON.stringify({jsonrpc: '2.0', method, params}));
  }

  call<T>(method: string, params: unknown = {}): Promise<T> {
    if (!this.nx) {
      return Promise.reject(new Error('window.nx is only available in the Switch web applet'));
    }

    const id = this.nextId++;
    const payload = JSON.stringify({jsonrpc: '2.0', id, method, params});
    return new Promise<T>((resolve, reject) => {
      const timeout = window.setTimeout(() => {
        this.pending.delete(id);
        this.debug('timeout id=' + id + ' ' + method);
        reject(new Error(`Native RPC timed out: ${method}`));
      }, 10000);

      this.pending.set(id, {
        resolve: value => resolve(value as T),
        reject,
        timeout
      });
      this.send(payload, () => {
        const request = this.pending.get(id);
        if (request) {
          window.clearTimeout(request.timeout);
          this.pending.delete(id);
          request.reject(new Error(`Native RPC queue stayed full: ${method}`));
        }
      });
    });
  }

  onNotification(listener: (message: RpcMessage) => void): () => void {
    this.notificationListeners.add(listener);
    return () => this.notificationListeners.delete(listener);
  }

  private send(payload: string, onFailure?: () => void, attempt = 0): void {
    if (!this.nx) {
      onFailure?.();
      return;
    }
    if (this.nx.sendMessage(payload)) {
      return;
    }
    if (attempt >= 100) {
      onFailure?.();
      return;
    }
    window.setTimeout(() => this.send(payload, onFailure, attempt + 1), 20);
  }

  /** Returns whether the message changed anything worth rendering. */
  private handleMessage(payload: string): boolean {
    // Native sends the NUL of its C string as part of the message body, and
    // JSON.parse rejects that trailing byte.
    const text = payload.replace(/\u0000+$/, '').trim();
    if (!text) {
      return false;
    }

    let message: RpcMessage;
    try {
      message = JSON.parse(text) as RpcMessage;
    } catch {
      this.debug('parse failed: ' + text.slice(0, 96));
      return false;
    }

    // Channel pump from native. It carries nothing and arrives up to ten times
    // a second, so it must never log, render, or reach listeners.
    if (message.method === 'app.ping') {
      return false;
    }

    this.debug(
        'recv ' +
        (message.id === undefined ? String(message.method) : 'id=' + message.id));

    if (message.id !== undefined) {
      const request = this.pending.get(message.id);
      if (!request) {
        this.debug('no pending request for id=' + message.id);
        return false;
      }
      window.clearTimeout(request.timeout);
      this.pending.delete(message.id);
      this.debug('resolved id=' + message.id);
      if (message.error) {
        request.reject(new Error(`${message.error.message} (${message.error.code})`));
      } else {
        request.resolve(message.result);
      }
      return true;
    }

    if (message.method === 'app.ready') {
      this.send(JSON.stringify({jsonrpc: '2.0', method: 'app.connected'}));
      this.bridgeConnected = true;
      if (this.readyTimeout !== undefined) {
        window.clearTimeout(this.readyTimeout);
        this.readyTimeout = undefined;
      }
      this.readyResolve?.();
      this.readyResolve = undefined;
      this.readyReject = undefined;
    }

    this.notificationListeners.forEach(listener => listener(message));
    return true;
  }

  /**
   * Reports a diagnostic line to the native log. Native only prints these when
   * the NRO was launched by nxlink, and never replies to them.
   */
  private debug(text: string): void {
    this.send(
        JSON.stringify({jsonrpc: '2.0', method: 'dev.log', params: {text}}));
  }

  private renderNow(): void {
    try {
      this.injector.get(ApplicationRef).tick();
    } catch {
      // tick() throws if a change detection pass is already running, which
      // means the view is being rendered anyway.
    }
  }

  private failReady(error: Error): void {
    if (this.readyTimeout !== undefined) {
      window.clearTimeout(this.readyTimeout);
      this.readyTimeout = undefined;
    }
    this.readyReject?.(error);
    this.readyResolve = undefined;
    this.readyReject = undefined;
    this.readyPromise = undefined;
  }
}
