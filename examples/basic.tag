<demo-widget>
  <p if={ visible } onclick="toggle">{ message }</p>
  <input value={ name } />
  <ul>
    <li each={ item, i in items } class={ active: item.active }>{ i }: { item.name }</li>
  </ul>

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
