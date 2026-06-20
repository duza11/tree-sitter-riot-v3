<demo-widget>
  <p if={ visible } onclick="toggle">{ message }</p>
  <input value={ name } />

  <style type="scss">
    $color: red;
    p { color: $color; }
  </style>

  <script>
    this.message = 'hello'
  </script>

  this.count = 0
  increment() {
    if (this.count < 10) this.count++
  }
</demo-widget>
