var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Runs inside the config page (Clay serializes it), so it must not use
// anything from this file's scope.
function customFn() {
  var clay = this;
  clay.on(clay.EVENTS.AFTER_BUILD, function() {
    // Progress bar: steps mode shows a Target instead of the label format.
    var mode = clay.getItemByMessageKey('ProgressMode');
    var labels = clay.getItemByMessageKey('LabelFormat');
    var target = clay.getItemByMessageKey('StepTarget');
    function showForMode() {
      var steps = mode.get() === '2';
      if (steps) { labels.hide(); target.show(); } else { labels.show(); target.hide(); }
    }
    mode.on('change', showForMode);
    showForMode();
  });

  // Custom period: show what the chosen "Active on" needs, check the times.
  clay.on(clay.EVENTS.AFTER_BUILD, function() {
    var repeat = clay.getItemByMessageKey('PeriodRepeat');
    var date = clay.getItemByMessageKey('PeriodDate');
    var days = clay.getItemByMessageKey('PeriodWeekdays');
    var start = clay.getItemByMessageKey('PeriodStart');
    var end = clay.getItemByMessageKey('PeriodEnd');
    var format = clay.getItemByMessageKey('PeriodLabelFormat');

    function pad(n) {
      return (n < 10 ? '0' : '') + n;
    }
    // The date defaults to the day the page is opened.
    if (!date.get()) {
      var now = new Date();
      date.set(now.getFullYear() + '-' + pad(now.getMonth() + 1) + '-' + pad(now.getDate()));
    }

    function showFor() {
      var mode = repeat.get();
      var on = mode !== '0';
      [start, end, format].forEach(function(item) {
        if (on) { item.show(); } else { item.hide(); }
      });
      if (mode === '1') { date.show(); } else { date.hide(); }
      if (mode === '2') { days.show(); } else { days.hide(); }
      checkOrder();
    }
    // The browser blocks Save while the end is not after the start; only
    // when the period is on (a hidden field cannot show the message).
    function checkOrder() {
      var input = end.$manipulatorTarget[0];
      var bad = repeat.get() !== '0' && start.get() && end.get() && end.get() <= start.get();
      input.setCustomValidity(bad ? 'The end must be after the start.' : '');
    }

    repeat.on('change', showFor);
    start.on('change', checkOrder);
    end.on('change', checkOrder);
    showFor();
  });
}

// eslint-disable-next-line no-unused-vars
var clay = new Clay(clayConfig, customFn);
