import {
	Component,
	Input,
	OnInit
}                       from '@angular/core';

import {
	WidgetComponent
}                       from '../widget.component';

@Component( {
	selector: 'latestwidget',
	templateUrl: 'tpl/widgets/latest.html'
} )

export class WidgetLatestComponent implements OnInit {

	public editing: boolean = false;

	@Input( 'widget' ) public parent: WidgetComponent;

	public constructor(
	) {
	};

	public ngOnInit() {
	};

}
