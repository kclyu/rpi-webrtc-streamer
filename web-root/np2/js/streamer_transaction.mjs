import Constants from './constants.mjs';

// makeid from the below url
// https://stackoverflow.com/questions/1349404/generate-random-string-characters-in-javascript
function makeid(length) {
  var result = '';
  var characters =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  var charactersLength = characters.length;
  for (var i = 0; i < length; i++) {
    result += characters.charAt(Math.floor(Math.random() * charactersLength));
  }
  return result;
}

class StreamerTransaction {
  constructor(transaction_map, resolve, reject, timeoutval = 3000) {
    // total transaction id length is 8
    this._transactionId = makeid(12);
    transaction_map.set(this._transactionId, this);

    this._resolve = resolve;
    this._reject = reject;
    this._timer = setTimeout(
      function () {
        this.timeout();
      }.bind(this),
      timeoutval
    );
  }

  get id() {
    return this._transactionId;
  }

  _destroyTimer() {
    clearTimeout(this._timer);
    this._timer = null;
  }

  success(result) {
    this._destroyTimer();
    return this._resolve(result);
  }

  fail(error) {
    console.error('Transanction %s failed', this._transactionId);
    this._destroyTimer();
    return this._reject(error);
  }

  timeout() {
    console.error('Transanction %s timeout', this._transactionId);
    this._destroyTimer();
    return this._reject(new Error('Timeout'));
  }
}

export default StreamerTransaction;
