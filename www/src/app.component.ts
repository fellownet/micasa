import {
	Component,
	OnInit,
	OnDestroy
}                         from '@angular/core';
import {
	Router,
	NavigationEnd
}                         from '@angular/router';
import { Observable }     from 'rxjs/Observable';

import {
	Session,
	SessionService
}                         from './session/session.service';

declare var Highcharts: any;

@Component( {
	selector: 'body',
	templateUrl: 'tpl/app.html'
} )

export class AppComponent implements OnInit, OnDestroy {

	private _alive: boolean = true;

	public session: Observable<Session>;

	public constructor(
		private _router: Router,
		private _sessionService: SessionService
	) {
		// Make the session observable from the session service available to the template.
		this.session = _sessionService.session;

		// The defaults for all chart types are set here, the one place that is in common for all
		// widgets with charts.
		let animationDuration: number = 300;
		Highcharts.setOptions( {
			global: {
				timezoneOffset: new Date().getTimezoneOffset()
			},
			title: false,
			credits: false,
			rangeSelector: {
				buttons: [ {
					type: 'day',
					count: 1,
					text: '1d'
				}, {
					type: 'week',
					count: 1,
					text: '1w'
				}, {
					type: 'month',
					count: 1,
					text: '1m'
				}, {
					type: 'month',
					count: 3,
					text: '3m'
				}, {
					type: 'month',
					count: 6,
					text: '6m'
				}, {
					type: 'year',
					count: 1,
					text: '1y'
				} ],
				selected: 0,
				inputEnabled: false
			},
			navigator: {
				height: 75
			},
			scrollbar: {
				enabled: false
			},
			legend: {
				enabled: false
			},
			chart: {
				animation: {
					duration: animationDuration
				},
				zoomType: 'x',
				alignTicks: false
			},
			xAxis: {
				type: 'datetime',
				ordinal: false
			},
			yAxis: {
				startOnTick: false,
				endOnTick: false
			},
			plotOptions: {
				column: {
					color: '#3c8dbc',
					animation: {
						duration: animationDuration
					},
					pointPadding: 0.1, // space between bars when multiple series are used
					groupPadding: 0 // space beween group of series bars
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

	public ngOnInit() {
		// When subscribing to an event emitter we need to unsubscribe manually as opposed to the
		// router params observable that does the unsubscribing for us.
		this._router.events
			.filter( event => event instanceof NavigationEnd )
			.takeWhile( () => this._alive )
			.subscribe( event => {
				document.body.scrollTop = 0;
			} )
		;
	};

	public ngOnDestroy() {
		this._alive = false;
	};

}
