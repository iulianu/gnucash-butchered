#ifndef FPO_HPP
#define FPO_HPP

#include "ViewletView.hpp"
#include "gnc/Session.hpp"

#include <QWidget>
#include <QGridLayout>

namespace gnc
{

class ViewletView;

class FPO : public QWidget
{
    Q_OBJECT
public:
    explicit FPO(QWidget *parent = 0, QHBoxLayout *FPOLayout = NULL);
    ViewletView *leftViewlet;
    ViewletView *rightViewlet;
    ViewletView *defaultViewlet;

    Session m_session;

Q_SIGNALS:
    void sessionLoaded();

public Q_SLOTS:

};

} // END namespace gnc

#endif // FPO_HPP
