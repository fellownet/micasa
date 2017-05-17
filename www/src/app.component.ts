import {
	Component,
	OnInit,
	ViewEncapsulation
}                         from '@angular/core';
import { Observable }     from 'rxjs/Observable';

import {
	Session,
	SessionService
}                         from './session/session.service';
import { ScreensService } from './screens/screens.service';
import { ACL }            from './users/users.service';

declare var $: any;
declare var navigator: any;

declare var require: any;
const Highcharts = require( 'highcharts/highcharts.src.js' );

@Component( {
	selector: 'body',
	templateUrl: 'tpl/app.html',
	styleUrls: [
		'css/app.css'
	],
	encapsulation: ViewEncapsulation.None
} )

export class AppComponent implements OnInit {

	public session: Observable<Session>;
	public ACL = ACL;
	public screens: Observable<any[]>;

	public constructor(
		private _sessionService: SessionService,
		private _screensService: ScreensService
	) {
	};

	public ngOnInit() {
		var me = this;
		me.session = me._sessionService.session;

		// NOTE the getScreens method returns an observable that, once being subscribed to by the async pipe
		// from the template, returns the list of screens as being cached by the session service. Alterations
		// on the list of screens will end up in this cache too, and the ui will update itself accordingly.
		me.screens = me._screensService.getScreens();

		// The defaults for all chart types are set here, the one place that is in common for all
		// widgets with charts.
		let animationDuration: number = 300;
		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			},
			title: false,
			credits: false,
			legend: {
				enabled: false,
				itemStyle: {
					fontWeight: 'normal'
				}
			},
			chart: {
				animation: {
					duration: animationDuration
				},
				zoomType: 'x',
				alignTicks: true,
				resetZoomButton: {
					theme: {
						display: 'none'
					}
				}
			},
			xAxis: {
				type: 'datetime',
				title: {
					text: null
				}
			},
			plotOptions: {
				column: {
					color: '#3c8dbc',
					animation: {
						duration: animationDuration
					},
					pointPadding: 0.05,
					groupPadding: 0.1
				},
				spline: {
					color: '#3c8dbc',
					animation: {
						duration: animationDuration
					},
					lineWidth: 2,
					states: {
						hover: {
							lineWidth: 2,
							halo: false
						}
					},
					marker: {
						enabled: false,
						states: {
							hover: {
								enabled: true,
								symbol: 'circle',
								radius: 6,
								lineWidth: 2
							}
						}
					}
				}
			}
		} );
	};

	public toggleMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		} else {
			$( 'body' ).addClass( 'sidebar-open' );
		}
	};

	public hideMenu(): void {
		if ( $( 'body' ).hasClass( 'sidebar-open' ) ) {
			$( 'body' ).removeClass( 'sidebar-open' );
		}
	};

}
