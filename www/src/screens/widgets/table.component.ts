import {
	Component,
	Input,
	OnInit
}                       from '@angular/core';

import {
	WidgetComponent
}                       from '../widget.component';

@Component( {
	selector: 'tablewidget',
	templateUrl: 'tpl/widgets/table.html'
} )

export class WidgetTableComponent implements OnInit {

	@Input( 'widget' ) public parent: WidgetComponent;

	public editing: boolean = false;

	public constructor(
	) {
	};

	public ngOnInit() {
	};

}
