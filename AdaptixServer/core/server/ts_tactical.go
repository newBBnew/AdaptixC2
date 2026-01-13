package server

import (
	"AdaptixServer/core/connector"
)

func (ts *Teamserver) TsPresyncTacticalWorkflow() []interface{} {
	ts.WorkflowMutex.Lock()
	defer ts.WorkflowMutex.Unlock()

	if len(ts.CurrentWorkflow) == 0 && ts.WorkflowTargetAgents == "" {
		return nil
	}

	packet := CreateSpTacticalWorkflowSync("update", ts.CurrentWorkflow, ts.WorkflowTargetAgents)
	return []interface{}{packet}
}

func (ts *Teamserver) TsTacticalWorkflowUpdate(steps []connector.TacticalWorkflowStep, targets string) {
	ts.WorkflowMutex.Lock()
	ts.CurrentWorkflow = steps
	ts.WorkflowTargetAgents = targets
	ts.WorkflowMutex.Unlock()

	packet := CreateSpTacticalWorkflowSync("update", steps, targets)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsTacticalWorkflowClear() {
	ts.WorkflowMutex.Lock()
	ts.CurrentWorkflow = nil
	ts.WorkflowTargetAgents = ""
	ts.WorkflowMutex.Unlock()

	packet := CreateSpTacticalWorkflowSync("clear", nil, "")
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsTacticalAiSuggestion(content string) {
	packet := CreateSpTacticalAiSuggestion(content)
	ts.TsSyncAllClients(packet)
}
