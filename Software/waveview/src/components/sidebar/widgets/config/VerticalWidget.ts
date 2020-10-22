import { IWidget } from '../../../../interfaces/sidebar/sidebarInterfaces';

let MeasurementsWidget: IWidget = {
  "title": "Vertical",
  "blocks": [
    {
      "blockType": "AdjustChannel",
      "data": {
        "channel": 1
      }
    },
    {
      "blockType": "AdjustValue",
      "data": {
        "value": 1,
        "unit": "V",
        "showPerDiv": true
      }
    },
    {
      "blockType": "AdjustValue",
      "data": {
        "value": 0,
        "unit": "mV",
        "showPerDiv": false
      }
    }
  ]
}

export default MeasurementsWidget;