/*
 * Copyright 2001 Stefan Schimanski <1Stein@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ktimer.h"

#include <time.h>

#include <QProcess>
#include <QTimer>
#include <KConfigGroup>
#include <klineedit.h>
#include <ktoolinvocation.h>
#include <kstandardguiitem.h>
#include <QAction>
#include <kstandardaction.h>
#include "kstatusnotifieritem.h"
#include <KToolInvocation>
#include <KHelpClient>
#include <KGuiItem>
#include <KSharedConfig>

class KTimerJobItem : public QTreeWidgetItem {
public:
    // NB: 'parent' is 'm_list'
    KTimerJobItem( KTimerJob *job, QTreeWidget *parent )
        : QTreeWidgetItem()
    {
		parent->addTopLevelItem(this);
        m_job = job;
        m_error = false;
        update();
    }

    // NB: 'parent' is 'm_list'
    KTimerJobItem( KTimerJob *job, QTreeWidget * parent, QTreeWidgetItem *after )
        : QTreeWidgetItem() {
			int otherItemIndex = parent->indexOfTopLevelItem(after);
			parent->insertTopLevelItem(otherItemIndex + 1, this);
        m_job = job;
        m_error = false;
        update();
    }

    virtual ~KTimerJobItem() {
        delete m_job;
    }

    KTimerJob *job() { return m_job; }

    void setStatus( bool error ) {
        m_error = error;
        update();
    }

    void update()
    {
        // In theory, it would be better for >=10 hour timers if the time columns were right aligned.
        // ... if only they did not look so ugly as such. :(
        setTextAlignment(0, Qt::AlignRight);
        setTextAlignment(1, Qt::AlignRight);
        
        //--

        QString value=m_job->formatTime(m_job->value());
        QString delay=m_job->formatTime(m_job->delay());

        if (QString::compare(value, delay) || m_job->state() != KTimerJob::Stopped)
        {
            setText( 0, value );
        }
        else
        {
            setText( 0, QString(""));
        }

        if( m_error )
        {
            setIcon( 0, QIcon::fromTheme( QStringLiteral( "process-stop" )) );
        }
        else
        if ( m_job->state() == KTimerJob::Paused)
        {
            setIcon(0, QIcon::fromTheme( QStringLiteral( "media-playback-pause" )));
        }
        else
        {
            setIcon( 0, QPixmap() );
        }

        setText( 1, delay );

        // TODO: it would probably be better to have no icon for the 'stopped' state.
        switch( m_job->state() ) {
            case KTimerJob::Stopped: setIcon( 2, QIcon::fromTheme( QStringLiteral( "media-playback-stop" )) ); break;
            case KTimerJob::Paused: setIcon( 2, QIcon::fromTheme( QStringLiteral( "media-playback-pause" )) ); break;
            case KTimerJob::Started: setIcon( 2, QIcon::fromTheme( QStringLiteral( "arrow-right" )) ); break;
        }

        setText( 3, m_job->command() );
    }

private:
    bool m_error;
    KTimerJob *m_job;
};


/***************************************************************/


struct KTimerPrefPrivate
{
    QList<KTimerJob *> jobs;
};

KTimerPref::KTimerPref( QWidget *parent)
    : QDialog( parent )
{
    d = new KTimerPrefPrivate;

    setupUi(this);

    // set icons
    m_stop->setIcon( QIcon::fromTheme( QStringLiteral( "media-playback-stop" )) );
    m_pause->setIcon( QIcon::fromTheme( QStringLiteral( "media-playback-pause" )) );
    m_start->setIcon( QIcon::fromTheme( QStringLiteral( "arrow-right" )) );

    // create tray icon
    KStatusNotifierItem *tray = new KStatusNotifierItem(this);
    tray->setIconByName(QStringLiteral( "ktimer" ));
    tray->setCategory(KStatusNotifierItem::ApplicationStatus);
    tray->setStatus(KStatusNotifierItem::Active);

	// TODO: Somehow replace the 'are you sure?' nuisance....
	//KStandardAction::quit(this, SLOT(exit()), tray->d->actionCollection);

    // Set initial visibility states & column widths
    m_state->hide();
    m_settings->hide();
    //DNW: m_list->header()->resizeSection(1, 100);

    // Until we can optimize it, the 'state' column simply does not have a good cost/benefit.
    m_list->hideColumn(2);

    // set help button gui item
    KGuiItem::assign(m_help,KStandardGuiItem::help());

    // Exit
    QAction *exit = KStandardAction::quit(this, SLOT(exit()), this);
    addAction(exit);

    // connect
    connect(m_add, &QPushButton::clicked, this, &KTimerPref::add);
    connect(m_edit, &QPushButton::clicked, this, &KTimerPref::edit);
    connect(m_done, &QPushButton::clicked, this, &KTimerPref::edit);
    connect(m_remove, &QPushButton::clicked, this, &KTimerPref::remove);
    connect(m_help, &QPushButton::clicked, this, &KTimerPref::help);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &KTimerPref::currentChanged);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, &KTimerPref::currentDoubleClicked);
    loadJobs( KSharedConfig::openConfig().data() );

    show();
}


KTimerPref::~KTimerPref()
{
    delete d;
}

void KTimerPref::saveAllJobs() {
    saveJobs( KSharedConfig::openConfig().data() );
}


void KTimerPref::add()
{
    KTimerJob *job = new KTimerJob;
    KTimerJobItem *item = new KTimerJobItem( job, m_list );

    connect(job, &KTimerJob::delayChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::valueChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::stateChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::commandChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::onScheduleChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::onPauseChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::onResumeChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::onStopChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::onSuccessChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::onFailureChanged, this, &KTimerPref::jobChanged);
    connect(job, &KTimerJob::finished, this, &KTimerPref::jobFinished);

    job->setUser( item );

    // Qt drops currentChanged signals on first item (bug?)
    if( m_list->topLevelItemCount()==1 )
      currentChanged( item , NULL);

    m_list->setCurrentItem( item );
    m_list->update();
    m_settings->show();
}

void KTimerPref::edit()
{
    if (m_settings->isHidden())
    {
        m_settings->show();
    }
    else
    {
        m_settings->hide();
    }
}

void KTimerPref::remove()
{
    delete m_list->currentItem();
    m_list->update();
}

void KTimerPref::help()
{
    KHelpClient::invokeHelp();
}

// note, don't use old, but added it so we can connect to the new one
void KTimerPref::currentChanged( QTreeWidgetItem *i , QTreeWidgetItem * /* old */)
{
    KTimerJobItem *item = static_cast<KTimerJobItem*>(i);
    if( item ) {
        KTimerJob *job = item->job();

        m_state->setEnabled( true );
        m_settings->setEnabled( true );
        m_remove->setEnabled( true );
        m_delayH->disconnect();
        m_delayM->disconnect();
        m_delay->disconnect();
        m_loop->disconnect();
        m_one->disconnect();
        m_consecutive->disconnect();
        m_start->disconnect();
        m_pause->disconnect();
        m_stop->disconnect();
        m_counter->disconnect();
        m_slider->disconnect();
		m_commandLine->disconnect();
		m_commandLine->lineEdit()->disconnect();
		m_onSchedule->disconnect();
		m_onSchedule->lineEdit()->disconnect();
		m_onPause->disconnect();
		m_onPause->lineEdit()->disconnect();
		m_onResume->disconnect();
		m_onResume->lineEdit()->disconnect();
		m_onStop->disconnect();
		m_onStop->lineEdit()->disconnect();
		m_onSuccess->disconnect();
		m_onSuccess->lineEdit()->disconnect();
		m_onFailure->disconnect();
		m_onFailure->lineEdit()->disconnect();

        // Set hour, minute and second QSpinBoxes before we connect to signals.
        int h, m, s;
        job->secondsToHMS( job->delay(), &h, &m, &s );
        m_delayH->setValue( h );
        m_delayM->setValue( m );
        m_delay->setValue( s );

        connect( m_commandLine->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setCommand(QString)) );
        connect( m_onSchedule->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setOnSchedule(QString)) );
        connect( m_onPause->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setOnPause(QString)) );
        connect( m_onResume->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setOnResume(QString)) );
        connect( m_onStop->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setOnStop(QString)) );
        connect( m_onSuccess->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setOnSuccess(QString)) );
        connect( m_onFailure->lineEdit(), SIGNAL(textChanged(QString)), job, SLOT(setOnFailure(QString)) );

        connect(m_delayH, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KTimerPref::delayChanged);
        connect(m_delayM, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KTimerPref::delayChanged);
        connect(m_delay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KTimerPref::delayChanged);
        connect(m_loop, &QCheckBox::toggled, job, &KTimerJob::setLoop);
        connect(m_one, &QCheckBox::toggled, job, &KTimerJob::setOneInstance);
        connect(m_consecutive, &QCheckBox::toggled, job, &KTimerJob::setConsecutive);
        connect(m_stop, &QToolButton::clicked, job, &KTimerJob::stop);
        connect(m_pause, &QToolButton::clicked, job, &KTimerJob::pause);
        connect(m_start, &QToolButton::clicked, job, &KTimerJob::start);

        connect( m_slider, SIGNAL(sliderMoved(int)), job, SLOT(setValue(int)) );

        m_commandLine->lineEdit()->setText( job->command() );
        m_onSchedule->lineEdit()->setText(job->onSchedule());
        m_onPause->lineEdit()->setText(job->onPause());
        m_onResume->lineEdit()->setText(job->onResume());
        m_onStop->lineEdit()->setText(job->onStop());
        m_onSuccess->lineEdit()->setText(job->onSuccess());
        m_onFailure->lineEdit()->setText(job->onFailure());

        m_loop->setChecked( job->loop() );
        m_one->setChecked( job->oneInstance() );
        m_consecutive->setChecked( job->consecutive() );
        m_counter->display( (int)job->value() );
        m_slider->setMaximum( job->delay() );
        m_slider->setValue( job->value() );

    } else {
        m_state->setEnabled( false );
        m_settings->setEnabled( false );
        m_remove->setEnabled( false );
    }
}

void KTimerPref::currentDoubleClicked( QTreeWidgetItem *i, int column)
{
    KTimerJobItem *item = static_cast<KTimerJobItem*>(i);
    if( item )
    {
        item->job()->toggle();
    }
}

void KTimerJob::toggle()
{
    /*
    When a user double-clicks a row, what could they be wanting?
    (1) open the item for editing,
    (2) toggle the item between started & stopped,
    (3) toggle the item between started & paused,
    */

    if (state()==Started)
    {
        stop();
    }
    else
    {
        start();
    }
}

void KTimerPref::jobChanged( KTimerJob *job )
{
    KTimerJobItem *item = static_cast<KTimerJobItem*>(job->user());
    if( item ) {
        item->update();
        m_list->update();

        if( item==m_list->currentItem() ) {

            // XXX optimize
            m_slider->setMaximum( job->delay() );
            m_slider->setValue( job->value() );
            m_counter->display( (int)job->value() );
        }
    }
}


void KTimerPref::jobFinished( KTimerJob *job, bool error )
{
    KTimerJobItem *item = static_cast<KTimerJobItem*>(job->user());
    item->setStatus( error );
    if( m_list->itemBelow(m_list->currentItem())!=nullptr && (static_cast<KTimerJobItem*>(m_list->itemBelow( m_list->currentItem() )))->job()->consecutive() ) {
        m_list->setCurrentItem( m_list->itemBelow( m_list->currentItem() ) );
        (static_cast<KTimerJobItem*>(m_list->currentItem()))->job()->start();
    }
    m_list->update();
}

/* Hour/Minute/Second was changed. This slot calculates the seconds until we start
    the job and inform the current job */
void KTimerPref::delayChanged()
{
    KTimerJobItem *item = static_cast<KTimerJobItem*>(m_list->currentItem());
    if ( item )
	{
		KTimerJob *job = item->job();

		unsigned oldDelay=job->delay();
		unsigned oldValue=job->value();

		unsigned newDelay = job->timeToSeconds( m_delayH->value(), m_delayM->value(), m_delay->value() );
		job->setDelay( newDelay );

		//If the value was already maxed-out, keep it that way.
		//If the oldValue is beyond the current limit... truncate it.
		if (oldDelay == oldValue || oldValue > newDelay)
		{
			job->setValue(newDelay);
		}

		jobChanged(job);
    }
}

// Really quits the application
void KTimerPref::exit() {
    done(0);
    qApp->quit();
}

void KTimerPref::done(int result) {
    saveAllJobs();
    QDialog::done(result);
}

void KTimerPref::saveJobs( KConfig *cfg )
{
	const int nbList=m_list->topLevelItemCount();
	for (int num = 0; num < nbList; ++num)
	{
		KTimerJobItem *item = static_cast<KTimerJobItem*>(m_list->topLevelItem(num));
        item->job()->save( cfg, QStringLiteral( "Job%1" ).arg( num ) );

	}

	KConfigGroup jobscfg = cfg->group("Jobs");
    jobscfg.writeEntry( "Number", m_list->topLevelItemCount());

    jobscfg.sync();
}

// BUG: misnomer... loadConfiguration() ?
void KTimerPref::loadJobs( KConfig *cfg )
{
	const KConfigGroup main=cfg->group("Jobs");
	
	showSeconds=main.readEntry("ShowSeconds", false);
	
    const int num = main.readEntry( "Number", 0 );
    for( int n=0; n<num; n++ ) {
            KTimerJob *job = new KTimerJob;
            KTimerJobItem *item = new KTimerJobItem( job, m_list );

            connect(job, &KTimerJob::delayChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::valueChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::stateChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::commandChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::onScheduleChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::onPauseChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::onResumeChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::onStopChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::onSuccessChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::onFailureChanged, this, &KTimerPref::jobChanged);
            connect(job, &KTimerJob::finished, this, &KTimerPref::jobFinished);

            job->load( cfg, QStringLiteral( "Job%1" ).arg(n) );

            job->setUser( item );
            jobChanged ( job);
    }

    m_list->update();
}


/*********************************************************************/


struct KTimerJobPrivate {
    unsigned delay;
    QString command;
    QString onSchedule;
    QString onPause;
    QString onResume;
    QString onStop;
    QString onSuccess;
    QString onFailure;
    bool loop;
    bool oneInstance;
    bool consecutive;
    unsigned value;
    KTimerJob::States state;
    QList<QProcess *> processes;
    void *user;

    QTimer *timer;
};


KTimerJob::KTimerJob( QObject *parent)
    : QObject( parent )
{
    d = new KTimerJobPrivate;

    d->delay = 100;
    d->loop = false;
    d->oneInstance = true;
    d->consecutive = false;
    d->value = 100;
    d->state = Stopped;
    d->user = 0;

    d->timer = new QTimer( this );
    connect(d->timer, &QTimer::timeout, this, &KTimerJob::timeout);
}


KTimerJob::~KTimerJob()
{
    delete d;
}


void KTimerJob::save( KConfig *cfg, const QString& grp )
{
	KConfigGroup groupcfg = cfg->group(grp);
    groupcfg.writeEntry( "Delay", d->delay );
    groupcfg.writePathEntry( "Command", d->command );
    groupcfg.writePathEntry( "OnSchedule", d->onSchedule );
    groupcfg.writePathEntry( "OnPause", d->onPause );
    groupcfg.writePathEntry( "OnResume", d->onResume );
    groupcfg.writePathEntry( "OnStop", d->onStop );
    groupcfg.writePathEntry( "OnSuccess", d->onSuccess );
    groupcfg.writePathEntry( "OnFailure", d->onFailure );
    groupcfg.writeEntry( "Loop", d->loop );
    groupcfg.writeEntry( "OneInstance", d->oneInstance );
    groupcfg.writeEntry( "Consecutive", d->consecutive );
    groupcfg.writeEntry( "State", (int)d->state );
    groupcfg.writeEntry( "Value", d->value );

    if (d->state == Started)
    {
        groupcfg.writeEntry("Expires", (int)(time(NULL)+d->value));
    }
    else
    {
        groupcfg.deleteEntry("Expires");
    }
}


void KTimerJob::load( KConfig *cfg, const QString& grp )
{
	KConfigGroup groupcfg = cfg->group(grp);
    setDelay( groupcfg.readEntry( "Delay", 100 ) );
    setCommand( groupcfg.readPathEntry( "Command", QString() ) );
    setOnSchedule( groupcfg.readPathEntry( "OnSchedule", QString() ) );
    setOnPause(    groupcfg.readPathEntry( "OnPause"   , QString() ) );
    setOnResume(   groupcfg.readPathEntry( "OnResume"  , QString() ) );
    setOnStop(     groupcfg.readPathEntry( "OnStop"    , QString() ) );
    setOnSuccess(  groupcfg.readPathEntry( "OnSuccess" , QString() ) );
    setOnFailure(  groupcfg.readPathEntry( "OnFailure" , QString() ) );

    setLoop( groupcfg.readEntry( "Loop", false ) );
    setOneInstance( groupcfg.readEntry( "OneInstance", d->oneInstance ) );
    setConsecutive( groupcfg.readEntry( "Consecutive", d->consecutive ) );
    setState( (States)groupcfg.readEntry( "State", (int)Stopped ) );

    int expireTime=groupcfg.readEntry( "Expires", (int)0);

    if (expireTime > 0 && state()==Started)
    {
        int newValue=expireTime-time(NULL);

        if (newValue > 0)
        {
            //There is time left on the countdown.... maybe not much!
            setValue( newValue );
        }
        else
        {
            //NB: the time has *EXPIRED* since we were last activated.... does the user still want it to run?
            //BUG: WIP: expired timers hold there old value on reload.
            setValue( groupcfg.readEntry("Value", d->delay) );
        }
    }
    else
    {
        //The timer position is where we left it.
        setValue( groupcfg.readEntry("Value", d->delay) );
    }
}


// Format given seconds to hour:minute:seconds and return QString
QString KTimerJob::formatTime( int seconds ) const
{
    int h, m, s;
    secondsToHMS( seconds, &h, &m, &s );

	// BUG: how can we get our preferences object here?
	bool showSeconds=false;
	
	if (showSeconds)
	{
		//IMO: having a bunch of out-of-sync changing-every-second "doomsday timers" is very distracting, and a bit anxiety provoking.
    	return QStringLiteral( "%1:%2:%3" ).arg( h ).arg( m, 2, 10, QLatin1Char( '0' ) ).arg( s,2, 10, QLatin1Char( '0' ) );
	}
	else
	if (h)
	{
		return QStringLiteral( "%1h %2m" ).arg(h).arg( m, 2, 10, QLatin1Char( '0' ) );
	}
	else
	if (m)
	{
		//return QStringLiteral( "%1m" ).arg( m, 2, 10, QLatin1Char( '0' ) );
		return QStringLiteral( "%1m" ).arg( m );
	}
	else
	{
		//return QStringLiteral( "%1s" ).arg( s, 2, 10, QLatin1Char( '0' ) );
		return QStringLiteral( "%1s" ).arg( s );
	}
}


// calculate seconds from hour, minute and seconds, returns seconds
int KTimerJob::timeToSeconds( int hours, int minutes, int seconds ) const
{
    return hours * 3600 + minutes * 60 + seconds;
}


// calculates hours, minutes and seconds from given secs.
void KTimerJob::secondsToHMS( int secs, int *hours, int *minutes, int *seconds ) const
{
    int s = secs;
    (*hours) = s / 3600;
    s = s % 3600;
    (*minutes) = s / 60;
    (*seconds) = s % 60;
}

void *KTimerJob::user()
{
    return d->user;
}


void KTimerJob::setUser( void *user )
{
    d->user = user;
}


unsigned KTimerJob::delay() const
{
    return d->delay;
}


void KTimerJob::pause()
{
    setState( Paused );
    fireInferior(d->onPause);
}

void KTimerJob::stop()
{
    setState( Stopped );
    fireInferior(d->onStop);
}

void KTimerJob::start()
{
    if (d->state==Paused) {
        fireInferior(d->onResume);
    } else {
        fireInferior(d->onSchedule);
    }

    setState( Started );
}

void KTimerJob::setDelay( int sec )
{
    setDelay( (unsigned)sec );
}

void KTimerJob::setValue( int value )
{
    setValue( (unsigned)value );
}

void KTimerJob::setDelay( unsigned sec )
{
    if( d->delay!=sec ) {
        d->delay = sec;

        if( d->state==Stopped )
            setValue( sec );

        emit delayChanged( this, sec );
        emit changed( this );
    }
}


QString KTimerJob::command() const
{
    return d->command;
}

QString KTimerJob::onSchedule() const
{
    return d->onSchedule;
}

QString KTimerJob::onPause() const
{
    return d->onPause;
}

QString KTimerJob::onResume() const
{
    return d->onResume;
}

QString KTimerJob::onStop() const
{
    return d->onStop;
}

QString KTimerJob::onSuccess() const
{
    return d->onSuccess;
}

QString KTimerJob::onFailure() const
{
    return d->onFailure;
}



void KTimerJob::setCommand( const QString &cmd )
{
    if( d->command!=cmd ) {
        d->command = cmd;
        emit commandChanged( this, cmd );
        emit changed( this );
    }
}

void KTimerJob::setOnSchedule( const QString &cmd )
{
    if( d->onSchedule!=cmd ) {
        d->onSchedule = cmd;
        emit onScheduleChanged( this, cmd );
        emit changed( this );
    }
}

void KTimerJob::setOnPause( const QString &cmd )
{
    if( d->onPause!=cmd ) {
        d->onPause = cmd;
        emit onPauseChanged( this, cmd );
        emit changed( this );
    }
}

void KTimerJob::setOnResume( const QString &cmd )
{
    if( d->onResume!=cmd ) {
        d->onResume = cmd;
        emit onResumeChanged( this, cmd );
        emit changed( this );
    }
}

void KTimerJob::setOnStop( const QString &cmd )
{
    if( d->onStop!=cmd ) {
        d->onStop = cmd;
        emit onStopChanged( this, cmd );
        emit changed( this );
    }
}

void KTimerJob::setOnSuccess( const QString &cmd )
{
    if( d->onSuccess!=cmd ) {
        d->onSuccess = cmd;
        emit onSuccessChanged( this, cmd );
        emit changed( this );
    }
}

void KTimerJob::setOnFailure( const QString &cmd )
{
    if( d->onFailure!=cmd ) {
        d->onFailure = cmd;
        emit onFailureChanged( this, cmd );
        emit changed( this );
    }
}



bool KTimerJob::loop() const
{
    return d->loop;
}


void KTimerJob::setLoop( bool loop )
{
    if( d->loop!=loop ) {
        d->loop = loop;
        emit loopChanged( this, loop );
        emit changed( this );
    }
}


bool KTimerJob::oneInstance() const
{
    return d->oneInstance;
}


void KTimerJob::setOneInstance( bool one )
{
    if( d->oneInstance!=one ) {
        d->oneInstance = one;
        emit oneInstanceChanged( this, one );
        emit changed( this );
    }
}


bool KTimerJob::consecutive() const
{
    return d->consecutive;
}


void KTimerJob::setConsecutive( bool consecutive )
{
    if( d->consecutive!=consecutive ) {
        d->consecutive = consecutive;
        emit consecutiveChanged( this, consecutive );
        emit changed( this );
    }
}


unsigned KTimerJob::value() const
{
    return d->value;
}


void KTimerJob::setValue( unsigned value )
{
    if( d->value!=value ) {
        d->value = value;
        emit valueChanged( this, value );
        emit changed( this );
    }
}


KTimerJob::States KTimerJob::state() const
{
    return d->state;
}


void KTimerJob::setState( KTimerJob::States state )
{
    if( d->state!=state ) {
        if( state==Started )
            d->timer->start( 1000 );
        else
            d->timer->stop();

        if( state==Stopped )
            setValue( d->delay );

        d->state = state;
        emit stateChanged( this, state );
        emit changed( this );
    }
}


void KTimerJob::timeout()
{
    if( d->state==Started && d->value!=0 ) {
        setValue( d->value-1 );
        if( d->value==0 ) {
            fire();
            if( d->loop )
                setValue( d->delay );
            else
                stop();
        }
    }
}


void KTimerJob::processExited(int, QProcess::ExitStatus status)
{
	QProcess * proc = static_cast<QProcess*>(sender());
    const bool ok = status==0;
    const int i = d->processes.indexOf( proc);
    if (i != -1)
        delete d->processes.takeAt(i);

    if( ok ) {
        fireInferior(d->onSuccess);
    } else {
        emit error( this );
        fireInferior(d->onFailure);
    }

    emit finished( this, !ok );
}


void KTimerJob::fire()
{
    if( !d->oneInstance || d->processes.isEmpty() ) {
        QProcess *proc = new QProcess;
        d->processes.append( proc );
        connect(proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &KTimerJob::processExited);
        if (!d->command.simplified ().isEmpty()) {
	        proc->start(d->command);
	        emit fired( this );
        }
        if(proc->state() == QProcess::NotRunning) {
            const int i = d->processes.indexOf( proc);
            if (i != -1)
                delete d->processes.takeAt(i);
            emit error( this );
            emit finished( this, true );
        }
    }
}

void KTimerJob::fireInferior(const QString &command)
{
    if (!command.simplified().isEmpty()) {
        QProcess *proc = new QProcess;
	    proc->start(command);
    }
}

