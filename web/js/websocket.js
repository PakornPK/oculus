/**
 * Real-time data client using Server-Sent Events (SSE).
 * Connects to /ws/rom for streaming ROM angle data.
 */
class OculusWebSocket {
    constructor(url) {
        this.url = url || `/ws/rom`;
        this.es = null;
        this.handlers = {};
        this.reconnectDelay = 1000;
        this.maxReconnectDelay = 30000;
        this.reconnectTimer = null;
    }

    connect() {
        if (this.es) {
            this.es.close();
            this.es = null;
        }

        try {
            this.es = new EventSource(this.url);
        } catch (e) {
            this._scheduleReconnect();
            return;
        }

        this.es.onopen = () => {
            this.reconnectDelay = 1000;
            this._emit('connected');
        };

        this.es.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this._emit('data', data);
                if (data.type) {
                    this._emit(data.type, data);
                }
            } catch (e) {
                this._emit('error', { message: 'Invalid JSON from server' });
            }
        };

        this.es.onerror = () => {
            this.es.close();
            this.es = null;
            this._emit('disconnected');
            this._scheduleReconnect();
        };
    }

    disconnect() {
        clearTimeout(this.reconnectTimer);
        if (this.es) {
            this.es.onopen = null;
            this.es.onerror = null;
            this.es.close();
            this.es = null;
        }
    }

    on(event, handler) {
        if (!this.handlers[event]) this.handlers[event] = [];
        this.handlers[event].push(handler);
        return this;
    }

    off(event, handler) {
        if (!this.handlers[event]) return this;
        if (!handler) {
            delete this.handlers[event];
        } else {
            this.handlers[event] = this.handlers[event].filter(h => h !== handler);
        }
        return this;
    }

    _emit(event, data) {
        const handlers = this.handlers[event];
        if (handlers) {
            handlers.forEach(h => h(data));
        }
    }

    _scheduleReconnect() {
        clearTimeout(this.reconnectTimer);
        this.reconnectTimer = setTimeout(() => {
            this.connect();
        }, this.reconnectDelay);
        this.reconnectDelay = Math.min(this.reconnectDelay * 2, this.maxReconnectDelay);
    }

    get connected() {
        return this.es && this.es.readyState === EventSource.OPEN;
    }
}

window.OculusWebSocket = OculusWebSocket;