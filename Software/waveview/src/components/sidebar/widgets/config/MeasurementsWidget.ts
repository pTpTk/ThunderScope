import { IWidget } from '../../../../interfaces/sidebar/sidebarInterfaces';
import BlockType from '../../../../interfaces/sidebar/blockType';

let MeasurementsWidget: IWidget =
{
  "title": "Measurements",
  "blocks": [
    {
      "blockType": BlockType.DisplayValue,
      "data": {
        "leftValue": "X1->X2",
        "rightValue": "500ns"
      }
    },
    {
      "blockType": BlockType.DisplayValue,
      "data": {
        "leftValue": "Y1->Y2",
        "rightValue": "300mV"
      }
    }
  ]
}

export default MeasurementsWidget;