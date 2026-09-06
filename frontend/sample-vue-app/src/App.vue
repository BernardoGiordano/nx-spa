<script setup lang="ts">
import { onMounted, ref } from 'vue'

import { NxBridge } from './nxBridge'

interface SystemInfo {
  firmware: string
  atmosphere: boolean
  offline: boolean
  transport: string
}

const bridge = new NxBridge()
const bridgeConnected = ref(false)
const first = ref(3)
const second = ref(4)
const sumResult = ref<number>()
const systemInfo = ref<SystemInfo>()
const lifecycle = ref('active')
const calculating = ref(false)
const error = ref('')

async function calculate(): Promise<void> {
  error.value = ''
  calculating.value = true
  try {
    sumResult.value = await bridge.call<number>('math.sum', {
      a: Number(first.value),
      b: Number(second.value)
    })
  } catch (reason) {
    error.value = reason instanceof Error ? reason.message : String(reason)
  } finally {
    calculating.value = false
  }
}

onMounted(async () => {
  if (!bridge.available) return
  bridge.onNotification(message => {
    if (message.method === 'app.lifecycle') {
      lifecycle.value = (message.params as { state?: string }).state ?? 'active'
    }
  })
  try {
    await bridge.connect('vue')
    bridgeConnected.value = true
    systemInfo.value = await bridge.call<SystemInfo>('system.info')
    await calculate()
  } catch (reason) {
    error.value = reason instanceof Error ? reason.message : String(reason)
  }
})
</script>

<template>
  <main>
    <header>
      <div>
        <p class="eyebrow">Nintendo Switch · offline SPA</p>
        <h1>nx-spa</h1>
        <p class="subtitle">Vue UI, native C++ backend, no HTTP server.</p>
      </div>
      <div class="status" :class="{ connected: bridgeConnected }">
        <span></span>
        {{ !bridge.available ? 'Browser preview' : bridgeConnected ? 'Native bridge connected' : 'Connecting native bridge…' }}
      </div>
    </header>

    <section class="architecture">
      <div><strong>Bundled assets</strong><small>NRO RomFS</small></div>
      <b>→</b>
      <div><strong>Offline browser</strong><small>HtmlDocument</small></div>
      <b>⇄</b>
      <div><strong>Dynamic API</strong><small>WebSession RPC</small></div>
    </section>

    <div class="grid">
      <section class="card">
        <p class="label">Native RPC demo</p>
        <h2>Add two numbers in C++</h2>
        <div class="calculator">
          <input v-model.number="first" type="number" inputmode="decimal" aria-label="First number">
          <span>+</span>
          <input v-model.number="second" type="number" inputmode="decimal" aria-label="Second number">
          <button type="button" :disabled="!bridgeConnected || calculating" @click="calculate">
            {{ calculating ? 'Calling…' : 'Call native' }}
          </button>
        </div>
        <div class="result"><small>Response</small><strong>{{ sumResult ?? '—' }}</strong></div>
        <p v-if="error" class="error">{{ error }}</p>
      </section>

      <section class="card">
        <p class="label">Runtime</p>
        <h2>Self-contained session</h2>
        <dl>
          <div><dt>Firmware</dt><dd>{{ systemInfo?.firmware ?? '—' }}</dd></div>
          <div><dt>Transport</dt><dd>{{ systemInfo?.transport ?? 'WebSession' }}</dd></div>
          <div><dt>Network</dt><dd>{{ systemInfo?.offline ? 'Not required' : '—' }}</dd></div>
          <div><dt>Lifecycle</dt><dd>{{ lifecycle }}</dd></div>
        </dl>
      </section>
    </div>

    <footer>
      The native process keeps auto-sleep disabled while this applet is open and restores the previous setting on exit.
    </footer>
  </main>
</template>

<style scoped>
main { width: min(1120px, calc(100% - 64px)); margin: 0 auto; padding: 48px 0 30px; }
header { display: flex; align-items: flex-start; justify-content: space-between; gap: 32px; }
.eyebrow, .label { margin: 0 0 8px; color: #5ee6c4; font-size: 13px; font-weight: 700; letter-spacing: .12em; text-transform: uppercase; }
h1 { margin: 0; font-size: clamp(48px, 8vw, 80px); line-height: .95; letter-spacing: -.055em; }
.subtitle { margin: 15px 0 0; color: #9eacc3; font-size: 18px; }
.status { display: flex; align-items: center; gap: 9px; margin-top: 6px; padding: 10px 14px; border: 1px solid rgba(173,205,255,.16); border-radius: 999px; color: #9eacc3; background: rgba(8,17,31,.55); font-size: 13px; white-space: nowrap; }
.status span { width: 8px; height: 8px; border-radius: 50%; background: #768197; }
.status.connected { color: #f5f8ff; }
.status.connected span { background: #5ee6c4; box-shadow: 0 0 16px #5ee6c4; }
.architecture { display: flex; align-items: center; justify-content: center; gap: 18px; margin: 42px 0 24px; padding: 16px 24px; border: 1px solid rgba(173,205,255,.16); border-radius: 16px; background: rgba(8,17,31,.52); }
.architecture div { display: flex; flex-direction: column; min-width: 160px; text-align: center; }
.architecture b { color: #5ee6c4; font-size: 24px; }
.architecture small, footer { color: #9eacc3; }
.grid { display: grid; grid-template-columns: 1.35fr 1fr; gap: 24px; }
.card { min-height: 275px; padding: 26px; border: 1px solid rgba(173,205,255,.16); border-radius: 22px; background: rgba(18,33,54,.86); box-shadow: 0 22px 60px rgba(0,0,0,.2); }
h2 { margin: 0 0 26px; font-size: 25px; letter-spacing: -.025em; }
.calculator { display: flex; align-items: center; gap: 12px; }
input, button { height: 48px; border-radius: 12px; font: inherit; font-size: 17px; }
input { width: 92px; padding: 0 13px; border: 1px solid rgba(173,205,255,.16); outline: none; color: #f5f8ff; background: #0b1728; }
input:focus { border-color: #5ee6c4; }
button { flex: 1; min-width: 130px; border: 0; color: #041815; background: #5ee6c4; font-weight: 800; }
button:disabled { opacity: .45; }
.result { display: flex; align-items: baseline; justify-content: space-between; margin-top: 22px; padding: 14px 18px; border-radius: 14px; background: #123f3b; }
.result small { color: #a9d9cf; }
.result strong { font-size: 28px; }
.error { margin: 12px 0 0; color: #ff9d9d; font-size: 13px; }
dl { margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 20px; padding: 12px 0; border-bottom: 1px solid rgba(173,205,255,.16); }
dl div:last-child { border-bottom: 0; }
dt { color: #9eacc3; }
dd { margin: 0; font-weight: 650; }
footer { margin-top: 22px; text-align: center; font-size: 13px; }
@media (max-width: 760px) {
  main { width: min(100% - 30px, 620px); padding-top: 28px; }
  header, .architecture { align-items: stretch; flex-direction: column; }
  .status { align-self: flex-start; }
  .architecture b { transform: rotate(90deg); text-align: center; }
  .architecture div { min-width: 0; }
  .grid { grid-template-columns: 1fr; }
  .calculator { flex-wrap: wrap; }
}
</style>
